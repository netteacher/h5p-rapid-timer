/**
 * H5P.RapidTimer – konfigurierbarer Zeitrahmen-Wrapper für beliebige H5P-Inhalte.
 * Anwendungsfall: Rapid Assessment (Kalyuga) – eine Frage, enges Zeitfenster,
 * Verzweigung Novize/Experte anhand von Richtigkeit UND Zeit.
 */
var H5P = H5P || {};

H5P.RapidTimer = (function ($, EventDispatcher) {
  'use strict';

  function RapidTimer(params, contentId, extras) {
    EventDispatcher.call(this);
    var self = this;

    self.params = $.extend(true, {
      timeLimit: 10,
      startMode: 'click',
      warningAt: 3,
      showBar: true,
      onTimeout: 'lock',
      timeoutMessage: 'Zeit abgelaufen – du startest im Einsteiger-Pfad.',
      successMessage: 'Stark! Du startest im Experten-Pfad.',
      startLabel: 'Start – Aufgabe anzeigen',
      retryLabel: '',
      expertUrl: '',
      expertLinkLabel: 'Weiter zum Expertenpfad →',
      noviceUrl: '',
      noviceLinkLabel: 'Weiter zum Einsteigerpfad →',
      redirectMode: 'link',
      redirectDelay: 4,
      confidence: {
        enabled: false,
        timing: 'after',
        coverText: 'Antwort erfasst – gleich siehst du die Lösung.',
        question: 'Wie sicher bist du, dass deine Antwort richtig ist?',
        fbConfCorrect: 'Einschätzung und Ergebnis passen zusammen – du kennst deinen Stand. Weiter im Expertenpfad.',
        fbConfWrong: 'Du warst sicher – die Lösung sagt etwas anderes. Genau hier steckt oft eine typische Fehlannahme. Der Einsteigerpfad räumt sie auf.',
        fbUnsureCorrect: 'Du kannst mehr, als du dir zutraust – das war richtig, und zwar in der Zeit. Trau dich in den Expertenpfad.',
        fbUnsureWrong: 'Deine Einschätzung war ehrlich und realistisch – eine gute Basis. Der Einsteigerpfad baut das Wissen Schritt für Schritt auf.'
      }
    }, params || {});

    self.contentId = contentId;
    self.extras = extras || {};

    // Zustand
    self.remaining = self.params.timeLimit;
    self.expired = false;
    self.answered = false;
    self.correctInTime = null; // true/false nach Antwort bzw. Timeout
    self.confident = null;     // Selbsteinschätzung: true=sicher, false=unsicher, null=nicht erhoben
    self.interval = null;
    self.child = null;
  }

  RapidTimer.prototype = Object.create(EventDispatcher.prototype);
  RapidTimer.prototype.constructor = RapidTimer;

  /* ---------- Rendering ---------- */

  RapidTimer.prototype.attach = function ($container) {
    var self = this;
    self.$container = $container.addClass('h5p-rapid-timer');

    // Kopf: Countdown
    self.$head = $('<div class="hrt-head" aria-live="polite"></div>').appendTo($container);
    self.$digits = $('<span class="hrt-digits"></span>').appendTo(self.$head);
    if (self.params.showBar) {
      self.$barWrap = $('<div class="hrt-bar"><div class="hrt-bar-fill"></div></div>').appendTo(self.$head);
      self.$barFill = self.$barWrap.find('.hrt-bar-fill');
    }

    // Statuszeile (Timeout-/Erfolgsmeldung)
    self.$status = $('<div class="hrt-status" role="alert"></div>').appendTo($container);

    // Inhaltsbereich
    self.$stage = $('<div class="hrt-stage"></div>').appendTo($container);
    self.$contentWrap = $('<div class="hrt-content"></div>').appendTo(self.$stage);
    self.$overlay = $('<div class="hrt-overlay" aria-hidden="true"></div>').appendTo(self.$stage);

    self.renderDigits();

    if (self.params.startMode === 'click') {
      // Inhalt erst nach Klick zeigen (Messfenster beginnt mit Sichtbarkeit)
      self.$contentWrap.addClass('hrt-hidden');
      self.$start = $('<button type="button" class="hrt-btn hrt-start"></button>')
        .text(self.params.startLabel || 'Start')
        .on('click', function () {
          self.$start.remove();
          self.begin();
        })
        .prependTo(self.$stage);
    }
    else {
      self.$contentWrap.addClass('hrt-hidden');
      self.begin();
    }

    // Optionaler Neustart
    if (self.params.retryLabel) {
      self.$retry = $('<button type="button" class="hrt-btn hrt-retry"></button>')
        .text(self.params.retryLabel)
        .on('click', function () { self.reset(); })
        .appendTo($container)
        .hide();
    }
  };

  /**
   * Ablaufsteuerung nach Start: erst optionale Sicherheitsabfrage, dann Aufgabe + Timer.
   * Der Timer läuft erst ab Sichtbarkeit der Aufgabe – die Einschätzung kostet keine Zeit.
   */
  RapidTimer.prototype.begin = function () {
    var self = this;
    var c = self.params.confidence;
    if (c && c.enabled && c.timing === 'before') {
      self.showConfidence(function () { self.reveal(); });
    }
    else {
      self.reveal();
    }
  };

  /**
   * Sicherheitsabfrage anzeigen. done() wird nach der Auswahl aufgerufen.
   */
  RapidTimer.prototype.showConfidence = function (done) {
    var self = this;
    self.$conf = $('<div class="hrt-confidence"></div>').prependTo(self.$stage);
    $('<p class="hrt-conf-q"></p>')
      .text(self.params.confidence.question || 'Wie sicher bist du?')
      .appendTo(self.$conf);
    var $row = $('<div class="hrt-conf-row"></div>').appendTo(self.$conf);

    var pick = function (isConfident) {
      self.confident = isConfident;
      self.$conf.remove();
      self.$conf = null;
      if (done) { done(); }
    };
    $('<button type="button" class="hrt-btn hrt-conf-btn hrt-conf-high">Sicher</button>')
      .on('click', function () { pick(true); }).appendTo($row);
    $('<button type="button" class="hrt-btn hrt-conf-btn hrt-conf-mid">Teils-teils</button>')
      .on('click', function () { pick(false); }).appendTo($row);
    $('<button type="button" class="hrt-btn hrt-conf-btn hrt-conf-low">Unsicher</button>')
      .on('click', function () { pick(false); }).appendTo($row);
    self.trigger('resize');
  };

  RapidTimer.prototype.reveal = function () {
    var self = this;
    self.$contentWrap.removeClass('hrt-hidden');
    self.attachChild();
    self.startTimer();
  };

  RapidTimer.prototype.attachChild = function () {
    var self = this;
    if (!self.params.content || !self.params.content.library) {
      self.$contentWrap.html('<p class="hrt-empty">Kein Inhalt konfiguriert – im Editor unter „Eingebetteter H5P-Inhalt" eine Aufgabe wählen.</p>');
      return;
    }
    self.child = H5P.newRunnable(
      self.params.content,
      self.contentId,
      self.$contentWrap,
      true,
      { parent: self }
    );

    // Antwort des Kindes auswerten
    self.child.on('xAPI', function (event) {
      var stmt = event.data && event.data.statement;
      if (!stmt || !stmt.verb) { return; }
      var verb = stmt.verb.id || '';
      if (verb.indexOf('answered') !== -1 && !self.answered) {
        self.answered = true;
        self.stopTimer();
        var res = stmt.result || {};
        var full = res.score && res.score.max > 0 && res.score.raw === res.score.max;
        self.correctInTime = (!self.expired && full);

        var c = self.params.confidence;
        if (c && c.enabled && c.timing === 'after' && !self.expired) {
          // Retrospektiv nach CBM-Prinzip: Lösung/Feedback des Kind-Elements
          // SOFORT verdecken, damit die Einschätzung nicht kontaminiert wird.
          self.coverContent(c.coverText || 'Antwort erfasst – gleich siehst du die Lösung.');
          self.showConfidence(function () {
            self.uncoverContent();
            self.finish(self.correctInTime);
          });
          self.trigger('resize');
        }
        else {
          self.finish(self.correctInTime);
        }
      }
    });

    self.trigger('resize');
  };

  /* ---------- Timer ---------- */

  RapidTimer.prototype.startTimer = function () {
    var self = this;
    self.stopTimer();
    self.remaining = self.params.timeLimit;
    self.renderDigits();
    self.interval = setInterval(function () {
      self.remaining--;
      self.renderDigits();
      if (self.params.warningAt > 0 && self.remaining <= self.params.warningAt) {
        self.$head.addClass('hrt-warning');
      }
      if (self.remaining <= 0) {
        self.onTimeout();
      }
    }, 1000);
  };

  RapidTimer.prototype.stopTimer = function () {
    if (this.interval) { clearInterval(this.interval); this.interval = null; }
  };

  RapidTimer.prototype.renderDigits = function () {
    var t = Math.max(0, this.remaining);
    var m = Math.floor(t / 60), s = t % 60;
    this.$digits.text((m > 0 ? m + ':' : '') + (m > 0 && s < 10 ? '0' : '') + s + (m > 0 ? '' : ' s'));
    if (this.$barFill) {
      this.$barFill.css('width', (t / this.params.timeLimit * 100) + '%');
    }
  };

  RapidTimer.prototype.onTimeout = function () {
    var self = this;
    self.stopTimer();
    self.expired = true;
    self.remaining = 0;
    self.renderDigits();
    self.$head.addClass('hrt-expired');

    if (self.params.onTimeout === 'lock') {
      self.$overlay.addClass('hrt-active');       // blockiert alle Eingaben
      self.correctInTime = false;
      self.finish(false);
    }
    else {
      // Hinweis-Modus: Aufgabe bleibt bedienbar, aber der Novizen-Link erscheint sofort
      self.$status.text(self.params.timeoutMessage).addClass('hrt-show hrt-fail');
      self.showPathLink(false);
    }
  };

  /* ---------- Abschluss & Reporting ---------- */

  RapidTimer.prototype.finish = function (success) {
    var self = this;
    var msg;
    var c = self.params.confidence;
    if (c && c.enabled && self.confident !== null) {
      // Kalibrierungs-Feedback: Selbsteinschätzung × Ergebnis
      msg = self.confident
        ? (success ? c.fbConfCorrect : c.fbConfWrong)
        : (success ? c.fbUnsureCorrect : c.fbUnsureWrong);
    }
    if (!msg) {
      msg = success ? self.params.successMessage : self.params.timeoutMessage;
    }
    self.$status
      .text(msg)
      .addClass('hrt-show ' + (success ? 'hrt-success' : 'hrt-fail'));
    if (self.$retry) { self.$retry.show(); }
    self.showPathLink(success);

    // Eigenes xAPI-Statement: Ergebnis inkl. Zeitkriterium (für Reports/Verzweigung)
    var xAPI = self.createXAPIEventTemplate('completed');
    xAPI.setScoredResult(success ? 1 : 0, 1, self, true, success);
    if (xAPI.data.statement.result) {
      xAPI.data.statement.result.duration =
        'PT' + (self.params.timeLimit - Math.max(0, self.remaining)) + 'S';
    }
    self.trigger(xAPI);
    self.trigger('resize');
  };

  /* ---------- Pfad-Weiterleitung (Novize/Experte) ---------- */

  RapidTimer.prototype.showPathLink = function (success) {
    var self = this;
    if (self.$pathLink) { return; } // nur einmal anzeigen

    var url = success ? self.params.expertUrl : self.params.noviceUrl;
    var label = success ? self.params.expertLinkLabel : self.params.noviceLinkLabel;
    if (!url) { return; }

    self.$pathLink = $('<a></a>')
      .addClass('hrt-btn hrt-path ' + (success ? 'hrt-path-expert' : 'hrt-path-novice'))
      .attr({ href: url, target: '_top', rel: 'noopener' })
      .text(label || url)
      .appendTo(self.$container);

    if (self.params.redirectMode === 'auto') {
      var wait = Math.max(1, self.params.redirectDelay || 4);
      var $note = $('<div class="hrt-redirect-note"></div>').appendTo(self.$container);
      var tick = function () {
        $note.text('Automatische Weiterleitung in ' + wait + ' s …');
        if (wait-- <= 0) {
          clearInterval(self.redirectInterval);
          self.doRedirect(url);
        }
      };
      tick();
      self.redirectInterval = setInterval(tick, 1000);
    }
    self.trigger('resize');
  };

  RapidTimer.prototype.doRedirect = function (url) {
    try { window.top.location.href = url; }        // aus dem H5P-iframe heraus
    catch (e) { window.location.href = url; }      // Fallback bei Cross-Origin
  };

  /* ---------- Lösungs-Abdeckung (verhindert kontaminierte Selbsteinschätzung) ---------- */

  RapidTimer.prototype.coverContent = function (text) {
    var self = this;
    if (self.$cover) { return; }
    self.$contentWrap.css('position', 'relative');
    self.$cover = $('<div class="hrt-cover" aria-hidden="true"></div>')
      .append($('<span class="hrt-cover-note"></span>').text(text || ''))
      .appendTo(self.$contentWrap);
  };

  RapidTimer.prototype.uncoverContent = function () {
    if (this.$cover) { this.$cover.remove(); this.$cover = null; }
  };

  RapidTimer.prototype.reset = function () {
    var self = this;
    self.stopTimer();
    self.uncoverContent();
    if (self.redirectInterval) { clearInterval(self.redirectInterval); self.redirectInterval = null; }
    if (self.$pathLink) { self.$pathLink.remove(); self.$pathLink = null; }
    self.$container.find('.hrt-redirect-note').remove();
    if (self.$conf) { self.$conf.remove(); self.$conf = null; }
    self.confident = null;
    self.stopTimerStateReset();
  };

  RapidTimer.prototype.stopTimerStateReset = function () {
    var self = this;
    self.expired = false;
    self.answered = false;
    self.correctInTime = null;
    self.$head.removeClass('hrt-warning hrt-expired');
    self.$status.removeClass('hrt-show hrt-success hrt-fail').text('');
    self.$overlay.removeClass('hrt-active');
    if (self.$retry) { self.$retry.hide(); }
    self.$contentWrap.empty().addClass('hrt-hidden');
    self.begin();
  };

  /* ---------- H5P-Question-Contract (Bewertung an Plattform) ---------- */

  RapidTimer.prototype.getAnswerGiven = function () {
    return this.answered || this.expired;
  };
  RapidTimer.prototype.getScore = function () {
    return this.correctInTime ? 1 : 0;
  };
  RapidTimer.prototype.getMaxScore = function () {
    return 1;
  };
  RapidTimer.prototype.getTitle = function () {
    return (this.extras.metadata && this.extras.metadata.title) || 'Rapid Timer';
  };

  return RapidTimer;
})(H5P.jQuery, H5P.EventDispatcher);
