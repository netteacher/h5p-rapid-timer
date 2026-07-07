/**
 * H5P.RapidTimer 2.0 – Rapid-Assessment-Serie nach Kalyuga.
 * Mehrere Items mit je eigenem hartem Zeitfenster, 3-Stufen-Konfidenz,
 * 3-Pfad-Klassifikation, Zustandspersistenz und erweitertem xAPI-Reporting.
 */
var H5P = H5P || {};

H5P.RapidTimer = (function ($, EventDispatcher) {
  'use strict';

  var EXT = {
    confidence: 'https://h5p-rapidtimer.local/xapi/confidence',
    timeUsed: 'https://h5p-rapidtimer.local/xapi/time-used',
    inTime: 'https://h5p-rapidtimer.local/xapi/in-time',
    classification: 'https://h5p-rapidtimer.local/xapi/classification'
  };

  function RapidTimer(params, contentId, extras) {
    EventDispatcher.call(this);
    var self = this;

    self.params = $.extend(true, {
      intro: {
        enabled: true,
        text: '<p><strong>So funktioniert dieser Schnelltest:</strong> Du bekommst mehrere kurze Aufgaben mit engem Zeitlimit. Das Zeitlimit ist Absicht – es prüft, ob du die Lösung sofort abrufen kannst. Wenn die Zeit nicht reicht oder etwas falsch ist, ist das kein Problem: Der Test führt dich danach genau zum passenden Lernmaterial.</p>'
      },
      items: [],
      practice: { enabled: false, timeLimit: 10 },
      timing: {
        startMode: 'click',
        advanceMode: 'click',
        warningAt: 3,
        showBar: true,
        onTimeout: 'lock',
        timeFactor: 1
      },
      confidence: {
        enabled: false,
        timing: 'after',
        question: 'Wie sicher bist du, dass deine Antwort richtig ist?',
        coverText: 'Antwort erfasst – gleich geht es weiter.',
        fbConfCorrect: 'Einschätzung und Ergebnis passen zusammen – du kennst deinen Stand.',
        fbConfWrong: 'Du warst sicher – die Lösung sagt etwas anderes. Genau hier steckt oft eine typische Fehlannahme. Lies die Erklärung besonders aufmerksam.',
        fbMidCorrect: 'Richtig gelöst, aber noch mit Restzweifel. Das Wissen ist da – es braucht noch Festigung, dann sitzt es sicher.',
        fbMidWrong: 'Dein Zweifel war berechtigt – hier fehlt noch ein Baustein. Die Erklärung zeigt dir, welcher.',
        fbUnsureCorrect: 'Du kannst mehr, als du dir zutraust – das war richtig, und zwar in der Zeit.',
        fbUnsureWrong: 'Deine Einschätzung war ehrlich und realistisch – eine gute Basis, um das Wissen Schritt für Schritt aufzubauen.'
      },
      classification: { expertMin: 80, intermediateMin: 40 },
      paths: {
        expertUrl: '', expertLabel: 'Weiter zum Expertenpfad →',
        intermediateUrl: '', intermediateLabel: 'Weiter zum Aufbaupfad →',
        noviceUrl: '', noviceLabel: 'Weiter zum Einsteigerpfad →',
        redirectMode: 'link',
        redirectDelay: 4
      },
      behaviour: {
        lockAfterCompletion: true,
        startLabel: 'Start – erste Aufgabe anzeigen',
        nextLabel: 'Weiter zur nächsten Aufgabe',
        progressLabel: 'Aufgabe @current von @total',
        timeoutMessage: 'Zeit abgelaufen – weiter geht\'s mit der nächsten Aufgabe.',
        summaryTitle: 'Deine Auswertung',
        expertMessage: 'Stark! Du hast das Wissen sicher und schnell abgerufen – der Expertenpfad ist der richtige für dich.',
        intermediateMessage: 'Solide Basis mit Lücken – der Aufbaupfad schließt sie gezielt.',
        noviceMessage: 'Der Einsteigerpfad baut das Wissen Schritt für Schritt auf – genau dafür ist er da.'
      }
    }, params || {});

    self.contentId = contentId;
    self.extras = extras || {};

    // Nur Items mit konfiguriertem Inhalt zählen.
    self.items = (self.params.items || []).filter(function (it) {
      return it && it.content && it.content.library;
    });

    // Laufzeit-Zustand
    self.results = self.items.map(function () { return null; });
    self.currentIndex = -1;      // Index des laufenden Items
    self.isPractice = false;
    self.practiceDone = false;
    self.finished = false;
    self.classification = null;  // 'expert' | 'intermediate' | 'novice'
    self.started = false;

    // Per-Item-Zustand
    self.expired = false;
    self.answered = false;
    self.confidenceLevel = null; // 'high' | 'mid' | 'low' | null
    self.timerEnd = null;
    self.timerStart = null;
    self.timerLimit = 0;
    self.interval = null;
    self.child = null;

    // Persistenz wiederherstellen
    var prev = self.extras.previousState;
    if (prev && prev.v === 2) {
      self.restoredState = prev;
    }
  }

  RapidTimer.prototype = Object.create(EventDispatcher.prototype);
  RapidTimer.prototype.constructor = RapidTimer;

  /* ---------- Rendering-Grundgerüst ---------- */

  RapidTimer.prototype.attach = function ($container) {
    var self = this;
    self.$container = $container.addClass('h5p-rapid-timer');

    // Resize von außen (Fenster/LMS) an das aktive Kind weiterreichen –
    // ohne das rechnen Typen mit absoluter Positionierung (z. B.
    // SingleChoiceSet) ihre Höhe nie aus und bleiben unsichtbar.
    self.on('resize', function () {
      if (self.child && !self.bubblingUpwards) {
        self.child.trigger('resize');
      }
    });

    // Kopf: Fortschritt + Countdown
    self.$head = $('<div class="hrt-head"></div>').appendTo($container);
    self.$progress = $('<span class="hrt-progress"></span>').appendTo(self.$head);
    self.$digits = $('<span class="hrt-digits" aria-hidden="true"></span>').appendTo(self.$head);
    if (self.params.timing.showBar) {
      self.$barWrap = $('<div class="hrt-bar"><div class="hrt-bar-fill"></div></div>').appendTo(self.$head);
      self.$barFill = self.$barWrap.find('.hrt-bar-fill');
    }
    // Gedrosselte Screenreader-Ansagen (nicht jede Sekunde)
    self.$live = $('<div class="hrt-sr-only" aria-live="polite"></div>').appendTo($container);

    // Statuszeile
    self.$status = $('<div class="hrt-status" role="alert"></div>').appendTo($container);

    // Bühne
    self.$stage = $('<div class="hrt-stage"></div>').appendTo($container);
    self.$contentWrap = $('<div class="hrt-content hrt-hidden"></div>').appendTo(self.$stage);
    self.$overlay = $('<div class="hrt-overlay" aria-hidden="true"></div>').appendTo(self.$stage);

    if (self.items.length === 0) {
      self.$stage.append('<p class="hrt-empty">Keine Aufgabe konfiguriert – im Editor unter „Aufgaben-Serie" mindestens einen Inhalt wählen.</p>');
      return;
    }

    // Wiederhergestellter Zustand?
    if (self.restoredState) {
      self.applyRestoredState(self.restoredState);
      return;
    }

    self.showIntro();
  };

  /* ---------- Persistenz ---------- */

  RapidTimer.prototype.getCurrentState = function () {
    var self = this;
    if (!self.started) { return; }
    return {
      v: 2,
      results: self.results,
      currentIndex: self.currentIndex,
      practiceDone: self.practiceDone,
      finished: self.finished,
      classification: self.classification
    };
  };

  RapidTimer.prototype.applyRestoredState = function (state) {
    var self = this;
    self.started = true;
    self.results = (state.results || []).slice(0, self.items.length);
    while (self.results.length < self.items.length) { self.results.push(null); }
    self.practiceDone = !!state.practiceDone;
    self.finished = !!state.finished;
    self.classification = state.classification || null;

    if (self.finished) {
      if (self.params.behaviour.lockAfterCompletion) {
        self.renderSummary();
      }
      else {
        self.renderSummary(true); // mit Neustart-Option
      }
      return;
    }

    // Mitten in der Serie unterbrochen: laufendes Item zählt als Timeout
    // (Timer-Garantie: kein Zeit-Cheat per Reload).
    var idx = typeof state.currentIndex === 'number' ? state.currentIndex : -1;
    if (idx >= 0 && idx < self.items.length && self.results[idx] === null) {
      self.results[idx] = {
        answered: false, timedOut: true, correct: false,
        scoreRaw: 0, scoreMax: 1, timeUsed: self.getItemLimit(idx), confidence: null
      };
    }
    var next = self.nextOpenIndex();
    if (next === -1) {
      self.finishSeries();
    }
    else {
      self.showResume(next);
    }
  };

  RapidTimer.prototype.nextOpenIndex = function () {
    for (var i = 0; i < this.results.length; i++) {
      if (this.results[i] === null) { return i; }
    }
    return -1;
  };

  RapidTimer.prototype.showResume = function (nextIndex) {
    var self = this;
    var $resume = $('<div class="hrt-transition"></div>').prependTo(self.$stage);
    $('<p class="hrt-transition-note"></p>')
      .text('Der Test wurde unterbrochen – die angefangene Aufgabe zählt als Zeitablauf.')
      .appendTo($resume);
    $('<button type="button" class="hrt-btn hrt-next"></button>')
      .text(self.params.behaviour.nextLabel || 'Weiter')
      .on('click', function () {
        $resume.remove();
        self.runItem(nextIndex);
      })
      .appendTo($resume);
    self.trigger('resize');
  };

  /* ---------- Intro & Start ---------- */

  RapidTimer.prototype.showIntro = function () {
    var self = this;
    var intro = self.params.intro || {};
    self.$intro = $('<div class="hrt-intro"></div>').prependTo(self.$stage);

    if (intro.enabled && intro.text) {
      $('<div class="hrt-intro-text"></div>').html(intro.text).appendTo(self.$intro);
    }

    var startSeries = function () {
      if (self.$intro) { self.$intro.remove(); self.$intro = null; }
      self.started = true;
      self.beginSeries();
    };

    if (self.params.timing.startMode === 'immediate' && !(intro.enabled && intro.text)) {
      // Start erst bei Sichtbarkeit im Viewport (Timer-Fairness)
      if (typeof IntersectionObserver !== 'undefined') {
        var observer = new IntersectionObserver(function (entries) {
          entries.forEach(function (entry) {
            if (entry.isIntersecting) {
              observer.disconnect();
              startSeries();
            }
          });
        }, { threshold: 0.5 });
        observer.observe(self.$container.get(0));
      }
      else {
        startSeries();
      }
    }
    else {
      $('<button type="button" class="hrt-btn hrt-start"></button>')
        .text(self.params.behaviour.startLabel || 'Start')
        .on('click', startSeries)
        .appendTo(self.$intro);
    }
    self.trigger('resize');
  };

  RapidTimer.prototype.beginSeries = function () {
    var self = this;
    var p = self.params.practice;
    if (p && p.enabled && p.content && p.content.library && !self.practiceDone) {
      self.runItem('practice');
    }
    else {
      self.runItem(0);
    }
  };

  /* ---------- Item-Ablauf ---------- */

  RapidTimer.prototype.getItemConfig = function (index) {
    if (index === 'practice') { return this.params.practice; }
    return this.items[index];
  };

  RapidTimer.prototype.getItemLimit = function (index) {
    var cfg = this.getItemConfig(index);
    var factor = Math.max(1, this.params.timing.timeFactor || 1);
    return Math.round((cfg.timeLimit || 10) * factor);
  };

  RapidTimer.prototype.runItem = function (index) {
    var self = this;
    self.isPractice = (index === 'practice');
    self.currentIndex = self.isPractice ? -1 : index;

    // Per-Item-Zustand zurücksetzen
    self.expired = false;
    self.answered = false;
    self.confidenceLevel = null;
    self.uncoverContent();
    self.$overlay.removeClass('hrt-active');
    self.$head.removeClass('hrt-warning hrt-expired');
    self.$status.removeClass('hrt-show hrt-success hrt-fail').text('');
    self.$contentWrap.empty().addClass('hrt-hidden');
    var wrapEl = self.$contentWrap.get(0);
    if (wrapEl && wrapEl.hasAttribute('inert')) { wrapEl.removeAttribute('inert'); }

    self.updateProgress();

    var c = self.params.confidence;
    if (c && c.enabled && c.timing === 'before') {
      self.showConfidence(function () { self.revealItem(index); });
    }
    else {
      self.revealItem(index);
    }
  };

  RapidTimer.prototype.updateProgress = function () {
    var self = this;
    if (self.isPractice) {
      self.$progress.text('Übungsaufgabe (zählt nicht)');
      return;
    }
    var tpl = self.params.behaviour.progressLabel || 'Aufgabe @current von @total';
    self.$progress.text(
      tpl.replace('@current', self.currentIndex + 1).replace('@total', self.items.length)
    );
  };

  RapidTimer.prototype.revealItem = function (index) {
    var self = this;
    var cfg = self.getItemConfig(index);

    self.$contentWrap.removeClass('hrt-hidden');
    self.child = H5P.newRunnable(cfg.content, self.contentId, self.$contentWrap, true, { parent: self });

    self.child.on('xAPI', function (event) {
      self.onChildXAPI(event, index);
    });

    // Resize des Kindes nach oben durchreichen (Schleifen-Schutz), damit
    // Layout-Änderungen im Kind den iframe/Container mitwachsen lassen.
    self.child.on('resize', function () {
      self.bubblingUpwards = true;
      self.trigger('resize');
      self.bubblingUpwards = false;
    });

    // Timer-Fairness: erst starten, wenn das Kind gerendert ist (nächster Frame
    // nach Attach) – langsames Rendering frisst keine Bearbeitungszeit.
    var raf = window.requestAnimationFrame || function (cb) { setTimeout(cb, 16); };
    raf(function () {
      raf(function () {
        self.startTimer(self.getItemLimit(index));
        // Kind explizit nachskalieren – Typen wie SingleChoiceSet setzen ihre
        // Höhe erst im eigenen resize-Handler.
        if (self.child) { self.child.trigger('resize'); }
        self.trigger('resize');
      });
    });
    self.trigger('resize');
  };

  RapidTimer.prototype.onChildXAPI = function (event, index) {
    var self = this;
    var stmt = event.data && event.data.statement;
    if (!stmt || !stmt.verb) { return; }
    var verb = stmt.verb.id || '';
    if (verb.indexOf('answered') === -1 || self.answered) { return; }

    self.answered = true;
    var timeUsed = self.stopTimer();
    var res = stmt.result || {};
    var raw = (res.score && typeof res.score.raw === 'number') ? res.score.raw : 0;
    var max = (res.score && typeof res.score.max === 'number') ? res.score.max : 0;
    var cfg = self.getItemConfig(index);
    var threshold = (cfg.scoreThreshold || 100) / 100;
    var correct = !self.expired && max > 0 && (raw / max) >= threshold;

    var record = {
      answered: true,
      timedOut: self.expired,
      correct: correct,
      scoreRaw: raw,
      scoreMax: max,
      timeUsed: timeUsed,
      confidence: self.confidenceLevel
    };

    var c = self.params.confidence;
    if (c && c.enabled && c.timing === 'after' && !self.expired) {
      // Lösung sofort verdecken, damit die Einschätzung nicht kontaminiert wird.
      self.coverContent(c.coverText || '');
      self.showConfidence(function () {
        record.confidence = self.confidenceLevel;
        self.uncoverContent();
        self.completeItem(index, record);
      });
      self.trigger('resize');
    }
    else {
      self.completeItem(index, record);
    }
  };

  /* ---------- Timer (Timestamp-basiert, nicht pausierbar) ---------- */

  RapidTimer.prototype.startTimer = function (limitSeconds) {
    var self = this;
    self.clearTimerInterval();
    self.timerLimit = limitSeconds;
    self.timerStart = Date.now();
    self.timerEnd = self.timerStart + limitSeconds * 1000;
    self.renderTime();
    self.announce('Der Timer läuft: ' + limitSeconds + ' Sekunden.');
    self.interval = setInterval(function () { self.tickTimer(); }, 250);
  };

  RapidTimer.prototype.tickTimer = function () {
    var self = this;
    var now = Date.now();
    self.renderTime();
    var remaining = Math.ceil((self.timerEnd - now) / 1000);
    var warnAt = self.params.timing.warningAt || 0;
    if (warnAt > 0 && remaining <= warnAt && !self.$head.hasClass('hrt-warning')) {
      self.$head.addClass('hrt-warning');
      self.announce('Noch ' + remaining + ' Sekunden.');
    }
    if (now >= self.timerEnd) {
      self.onTimeout();
    }
  };

  /** Stoppt den Timer und liefert die genutzte Zeit in Sekunden (1 Dezimale). */
  RapidTimer.prototype.stopTimer = function () {
    var self = this;
    self.clearTimerInterval();
    if (!self.timerStart) { return 0; }
    var used = Math.min(self.timerLimit, (Date.now() - self.timerStart) / 1000);
    return Math.round(used * 10) / 10;
  };

  RapidTimer.prototype.clearTimerInterval = function () {
    if (this.interval) { clearInterval(this.interval); this.interval = null; }
  };

  RapidTimer.prototype.renderTime = function () {
    var self = this;
    var msLeft = Math.max(0, (self.timerEnd || 0) - Date.now());
    var t = Math.ceil(msLeft / 1000);
    var m = Math.floor(t / 60), s = t % 60;
    self.$digits.text((m > 0 ? m + ':' : '') + (m > 0 && s < 10 ? '0' : '') + s + (m > 0 ? '' : ' s'));
    if (self.$barFill && self.timerLimit > 0) {
      self.$barFill.css('width', (msLeft / (self.timerLimit * 1000) * 100) + '%');
    }
  };

  RapidTimer.prototype.announce = function (text) {
    if (this.$live) { this.$live.text(text); }
  };

  RapidTimer.prototype.onTimeout = function () {
    var self = this;
    if (self.expired) { return; }
    self.clearTimerInterval();
    self.expired = true;
    self.renderTime();
    self.$head.addClass('hrt-expired');
    self.announce('Zeit abgelaufen.');

    var record = {
      answered: false, timedOut: true, correct: false,
      scoreRaw: 0, scoreMax: 1,
      timeUsed: self.timerLimit,
      confidence: self.confidenceLevel
    };

    if (self.params.timing.onTimeout === 'lock') {
      self.lockChild();
      self.completeItem(self.isPractice ? 'practice' : self.currentIndex, record);
    }
    else {
      // Hinweis-Modus: Aufgabe bleibt bedienbar, zählt aber als falsch.
      self.$status.text(self.params.behaviour.timeoutMessage).addClass('hrt-show hrt-fail');
      self.showSkipButton(record);
    }
  };

  RapidTimer.prototype.lockChild = function () {
    var self = this;
    self.$overlay.addClass('hrt-active');
    var wrapEl = self.$contentWrap.get(0);
    if (wrapEl) {
      try { wrapEl.setAttribute('inert', ''); }
      catch (e) { /* alte Browser: Overlay blockt Pointer, Fokus wird entzogen */ }
    }
    if (document.activeElement && self.$contentWrap.has(document.activeElement).length) {
      document.activeElement.blur();
    }
  };

  RapidTimer.prototype.showSkipButton = function (record) {
    var self = this;
    var index = self.isPractice ? 'practice' : self.currentIndex;
    self.$skip = $('<button type="button" class="hrt-btn hrt-next"></button>')
      .text(self.params.behaviour.nextLabel || 'Weiter')
      .on('click', function () {
        // Falls inzwischen doch geantwortet wurde, hat onChildXAPI übernommen.
        if (!self.answered) {
          self.completeItem(index, record);
        }
      })
      .appendTo(self.$container);
    self.trigger('resize');
  };

  /* ---------- Konfidenz (3 echte Stufen) ---------- */

  RapidTimer.prototype.showConfidence = function (done) {
    var self = this;
    self.$conf = $('<div class="hrt-confidence"></div>').prependTo(self.$stage);
    $('<p class="hrt-conf-q"></p>')
      .text(self.params.confidence.question || 'Wie sicher bist du?')
      .appendTo(self.$conf);
    var $row = $('<div class="hrt-conf-row"></div>').appendTo(self.$conf);

    var pick = function (level) {
      self.confidenceLevel = level;
      self.$conf.remove();
      self.$conf = null;
      if (done) { done(); }
    };
    $('<button type="button" class="hrt-btn hrt-conf-btn hrt-conf-high">Sicher</button>')
      .on('click', function () { pick('high'); }).appendTo($row);
    $('<button type="button" class="hrt-btn hrt-conf-btn hrt-conf-mid">Teils-teils</button>')
      .on('click', function () { pick('mid'); }).appendTo($row);
    $('<button type="button" class="hrt-btn hrt-conf-btn hrt-conf-low">Unsicher</button>')
      .on('click', function () { pick('low'); }).appendTo($row);
    self.trigger('resize');
  };

  RapidTimer.prototype.confidenceFeedback = function (result) {
    var c = this.params.confidence;
    if (!c || !c.enabled || !result.confidence) { return null; }
    var map = {
      high: { correct: c.fbConfCorrect, wrong: c.fbConfWrong },
      mid: { correct: c.fbMidCorrect, wrong: c.fbMidWrong },
      low: { correct: c.fbUnsureCorrect, wrong: c.fbUnsureWrong }
    };
    var entry = map[result.confidence];
    return entry ? (result.correct ? entry.correct : entry.wrong) : null;
  };

  /* ---------- Item-Abschluss & Übergang ---------- */

  RapidTimer.prototype.completeItem = function (index, record) {
    var self = this;
    if (self.$skip) { self.$skip.remove(); self.$skip = null; }

    if (index === 'practice') {
      self.practiceDone = true;
      self.showTransition('practice');
      return;
    }

    self.results[index] = record;
    self.triggerItemXAPI(index, record);

    var next = self.nextOpenIndex();
    if (next === -1) {
      self.finishSeries();
    }
    else {
      self.showTransition(next);
    }
  };

  /**
   * Neutrale Überleitung: KEIN richtig/falsch-Feedback zwischen den Items,
   * nächstes Item bleibt bis zum Timer-Start verdeckt.
   */
  RapidTimer.prototype.showTransition = function (next) {
    var self = this;
    self.child = null;
    self.$contentWrap.empty().addClass('hrt-hidden');
    self.uncoverContent();
    self.$overlay.removeClass('hrt-active');
    self.$head.removeClass('hrt-warning hrt-expired');
    self.$status.removeClass('hrt-show hrt-success hrt-fail').text('');

    var isPracticeDone = (next === 'practice');
    var nextIndex = isPracticeDone ? 0 : next;

    var $trans = $('<div class="hrt-transition"></div>').prependTo(self.$stage);
    var note = isPracticeDone
      ? 'Übung beendet – jetzt beginnt der eigentliche Test.'
      : 'Aufgabe erfasst.';
    $('<p class="hrt-transition-note"></p>').text(note).appendTo($trans);

    var proceed = function () {
      $trans.remove();
      self.runItem(nextIndex);
    };

    if (self.params.timing.advanceMode === 'auto' && !isPracticeDone) {
      var $count = $('<p class="hrt-redirect-note"></p>').appendTo($trans);
      var wait = 2;
      var tick = function () {
        $count.text('Nächste Aufgabe in ' + wait + ' s …');
        if (wait-- <= 0) {
          clearInterval(advInterval);
          proceed();
        }
      };
      tick();
      var advInterval = setInterval(tick, 1000);
    }
    else {
      $('<button type="button" class="hrt-btn hrt-next"></button>')
        .text(self.params.behaviour.nextLabel || 'Weiter')
        .on('click', proceed)
        .appendTo($trans);
    }
    self.trigger('resize');
  };

  /* ---------- Serien-Abschluss, Klassifikation & Summary ---------- */

  RapidTimer.prototype.correctCount = function () {
    return this.results.filter(function (r) { return r && r.correct; }).length;
  };

  RapidTimer.prototype.classify = function () {
    var self = this;
    var n = self.items.length;
    var pct = n > 0 ? (self.correctCount() / n) * 100 : 0;
    var cls = self.params.classification;
    if (pct >= (cls.expertMin || 80)) { return 'expert'; }
    // Mittelpfad nur, wenn eine URL konfiguriert ist (sonst 2-Pfad-Verhalten)
    if (self.params.paths.intermediateUrl && pct >= (cls.intermediateMin || 40)) {
      return 'intermediate';
    }
    return 'novice';
  };

  RapidTimer.prototype.finishSeries = function () {
    var self = this;
    self.finished = true;
    self.classification = self.classify();
    self.triggerFinalXAPI();
    self.renderSummary(!self.params.behaviour.lockAfterCompletion);
  };

  RapidTimer.prototype.renderSummary = function (allowRestart) {
    var self = this;
    self.child = null;
    self.clearTimerInterval();
    self.$head.hide();
    self.$status.removeClass('hrt-show hrt-success hrt-fail').text('');
    self.$stage.children('.hrt-transition, .hrt-intro, .hrt-confidence').remove();
    self.$contentWrap.empty().addClass('hrt-hidden');
    self.$overlay.removeClass('hrt-active');

    var b = self.params.behaviour;
    var $sum = $('<div class="hrt-summary"></div>').appendTo(self.$stage);

    $('<h2 class="hrt-summary-title"></h2>').text(b.summaryTitle || 'Auswertung').appendTo($sum);

    var n = self.items.length;
    var correct = self.correctCount();
    $('<p class="hrt-summary-score"></p>')
      .text(correct + ' von ' + n + ' Aufgaben in der Zeit richtig gelöst.')
      .appendTo($sum);

    var clsMsg = {
      expert: b.expertMessage,
      intermediate: b.intermediateMessage,
      novice: b.noviceMessage
    }[self.classification];
    $('<div class="hrt-summary-class"></div>')
      .addClass('hrt-class-' + self.classification)
      .text(clsMsg || '')
      .appendTo($sum);

    // Kalibrierungsübersicht
    if (self.params.confidence.enabled) {
      self.renderCalibration($sum);
    }

    // Item-Details: Hypercorrection-Fälle (sicher + falsch) zuerst
    self.renderItemReview($sum);

    // Pfad-Link
    self.showPathLink($sum);

    if (allowRestart) {
      $('<button type="button" class="hrt-btn hrt-retry">Test wiederholen</button>')
        .on('click', function () { self.restart(); })
        .appendTo($sum);
    }

    self.announce('Test abgeschlossen. ' + correct + ' von ' + n + ' Aufgaben richtig.');
    self.trigger('resize');
  };

  RapidTimer.prototype.renderCalibration = function ($parent) {
    var self = this;
    var buckets = { high: [0, 0], mid: [0, 0], low: [0, 0] }; // [gesamt, richtig]
    self.results.forEach(function (r) {
      if (r && r.confidence && buckets[r.confidence]) {
        buckets[r.confidence][0]++;
        if (r.correct) { buckets[r.confidence][1]++; }
      }
    });
    var labels = { high: 'Sicher', mid: 'Teils-teils', low: 'Unsicher' };
    var $cal = $('<div class="hrt-calibration"></div>').appendTo($parent);
    $('<h3 class="hrt-cal-title">Deine Selbsteinschätzung</h3>').appendTo($cal);
    var lines = [];
    ['high', 'mid', 'low'].forEach(function (k) {
      if (buckets[k][0] > 0) {
        lines.push(buckets[k][0] + '× „' + labels[k] + '" → ' + buckets[k][1] + '× richtig');
      }
    });
    $('<p class="hrt-cal-lines"></p>').text(lines.join(' · ')).appendTo($cal);

    // Einfache Deutungsheuristik
    var overconfident = buckets.high[0] - buckets.high[1]; // sicher, aber falsch
    var underconfident = buckets.low[1];                   // unsicher, aber richtig
    var verdict;
    if (overconfident > underconfident && overconfident > 0) {
      verdict = 'Tendenz zur Überschätzung: Achte besonders auf die markierten Aufgaben unten – dort steckt oft eine Fehlannahme.';
    }
    else if (underconfident > overconfident && underconfident > 0) {
      verdict = 'Tendenz zur Unterschätzung: Du kannst mehr, als du dir zutraust.';
    }
    else {
      verdict = 'Deine Einschätzung passt gut zu deinen Ergebnissen – gute Kalibrierung.';
    }
    $('<p class="hrt-cal-verdict"></p>').text(verdict).appendTo($cal);
  };

  RapidTimer.prototype.renderItemReview = function ($parent) {
    var self = this;
    var entries = self.results.map(function (r, i) { return { r: r, i: i }; })
      .filter(function (e) { return e.r !== null; });

    // Hypercorrection: sicher + falsch nach vorn
    entries.sort(function (a, b) {
      var aHyper = (a.r.confidence === 'high' && !a.r.correct) ? 0 : 1;
      var bHyper = (b.r.confidence === 'high' && !b.r.correct) ? 0 : 1;
      return aHyper - bHyper || a.i - b.i;
    });

    var $list = $('<div class="hrt-review"></div>').appendTo($parent);
    entries.forEach(function (e) {
      var r = e.r;
      var isHyper = (r.confidence === 'high' && !r.correct);
      var $item = $('<div class="hrt-review-item"></div>')
        .addClass(r.correct ? 'hrt-ri-correct' : 'hrt-ri-wrong')
        .toggleClass('hrt-ri-hyper', isHyper)
        .appendTo($list);

      var icon = r.correct ? '✓' : (r.timedOut && !r.answered ? '⏱' : '✗');
      var $head = $('<div class="hrt-ri-head"></div>').appendTo($item);
      $('<span class="hrt-ri-icon" aria-hidden="true"></span>').text(icon).appendTo($head);
      var headText = 'Aufgabe ' + (e.i + 1) + ': ' +
        (r.correct ? 'richtig (in der Zeit)' : (r.timedOut && !r.answered ? 'Zeit abgelaufen' : 'nicht richtig'));
      $('<span class="hrt-ri-label"></span>').text(headText).appendTo($head);

      var fb = self.confidenceFeedback(r);
      if (fb) {
        $('<p class="hrt-ri-feedback"></p>').text(fb).appendTo($item);
      }

      var explanation = self.items[e.i].explanation;
      if (explanation) {
        if (isHyper) {
          // Hypercorrection-Moment: Erklärung offen und hervorgehoben
          $('<div class="hrt-ri-explanation hrt-ri-explanation-open"></div>')
            .html(explanation).appendTo($item);
        }
        else {
          var $details = $('<details class="hrt-ri-details"></details>').appendTo($item);
          $('<summary>Erklärung anzeigen</summary>').appendTo($details);
          $('<div class="hrt-ri-explanation"></div>').html(explanation).appendTo($details);
          $details.on('toggle', function () { self.trigger('resize'); });
        }
      }
    });
  };

  /* ---------- Pfad-Weiterleitung ---------- */

  RapidTimer.prototype.showPathLink = function ($parent) {
    var self = this;
    var p = self.params.paths;
    var conf = {
      expert: { url: p.expertUrl, label: p.expertLabel },
      intermediate: { url: p.intermediateUrl, label: p.intermediateLabel },
      novice: { url: p.noviceUrl, label: p.noviceLabel }
    }[self.classification];
    if (!conf || !conf.url) { return; }

    $('<a></a>')
      .addClass('hrt-btn hrt-path hrt-path-' + self.classification)
      .attr({ href: conf.url, target: '_top', rel: 'noopener' })
      .text(conf.label || conf.url)
      .appendTo($parent);

    if (p.redirectMode === 'auto') {
      var wait = Math.max(1, p.redirectDelay || 4);
      var $note = $('<div class="hrt-redirect-note"></div>').appendTo($parent);
      var tick = function () {
        $note.text('Automatische Weiterleitung in ' + wait + ' s …');
        if (wait-- <= 0) {
          clearInterval(self.redirectInterval);
          self.doRedirect(conf.url);
        }
      };
      tick();
      self.redirectInterval = setInterval(tick, 1000);
    }
  };

  RapidTimer.prototype.doRedirect = function (url) {
    try { window.top.location.href = url; }
    catch (e) { window.location.href = url; }
  };

  /* ---------- Abdeckung (verhindert kontaminierte Selbsteinschätzung) ---------- */

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

  /* ---------- Neustart (nur wenn lockAfterCompletion aus) ---------- */

  RapidTimer.prototype.restart = function () {
    var self = this;
    if (self.redirectInterval) { clearInterval(self.redirectInterval); self.redirectInterval = null; }
    self.clearTimerInterval();
    self.results = self.items.map(function () { return null; });
    self.currentIndex = -1;
    self.finished = false;
    self.classification = null;
    self.practiceDone = false;
    self.$stage.children('.hrt-summary').remove();
    self.$head.show().removeClass('hrt-warning hrt-expired');
    self.showIntro();
  };

  /* ---------- xAPI ---------- */

  RapidTimer.prototype.triggerItemXAPI = function (index, record) {
    var self = this;
    var xAPI = self.createXAPIEventTemplate('answered');
    xAPI.setScoredResult(record.correct ? 1 : 0, 1, self, true, record.correct);
    var result = xAPI.data.statement.result;
    if (result) {
      result.duration = 'PT' + record.timeUsed + 'S';
      result.extensions = result.extensions || {};
      result.extensions[EXT.timeUsed] = record.timeUsed;
      result.extensions[EXT.inTime] = !record.timedOut;
      if (record.confidence) {
        result.extensions[EXT.confidence] = record.confidence;
      }
    }
    self.trigger(xAPI);
  };

  RapidTimer.prototype.triggerFinalXAPI = function () {
    var self = this;
    var n = self.items.length;
    var correct = self.correctCount();
    var totalTime = self.results.reduce(function (acc, r) {
      return acc + (r ? r.timeUsed : 0);
    }, 0);

    var xAPI = self.createXAPIEventTemplate('completed');
    xAPI.setScoredResult(correct, n, self, true, self.classification === 'expert');
    var result = xAPI.data.statement.result;
    if (result) {
      result.duration = 'PT' + Math.round(totalTime) + 'S';
      result.extensions = result.extensions || {};
      result.extensions[EXT.classification] = self.classification;
    }
    self.trigger(xAPI);
  };

  /* ---------- H5P-Question-Contract ---------- */

  RapidTimer.prototype.getAnswerGiven = function () {
    return this.finished || this.results.some(function (r) { return r !== null; });
  };
  RapidTimer.prototype.getScore = function () {
    return this.correctCount();
  };
  RapidTimer.prototype.getMaxScore = function () {
    return this.items.length;
  };
  RapidTimer.prototype.getTitle = function () {
    return (this.extras.metadata && this.extras.metadata.title) || 'Rapid Timer';
  };

  return RapidTimer;
})(H5P.jQuery, H5P.EventDispatcher);
