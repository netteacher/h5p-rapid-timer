/**
 * Content-Upgrade H5P.RapidTimer 1.x -> 2.0
 * Migriert Einzel-Item-Inhalte in die neue Serien-Struktur (Serie mit 1 Item).
 * Verhalten bleibt funktional identisch: 1 Item, 2 Pfade, 100 %-Schwelle, kein Intro.
 */
var H5PUpgrades = H5PUpgrades || {};

H5PUpgrades['H5P.RapidTimer'] = (function () {
  return {
    2: {
      0: function (parameters, finished, extras) {
        parameters = parameters || {};
        var oldConf = parameters.confidence || {};

        var newParams = {
          intro: {
            // Alte Inhalte hatten keine Intro-Seite -> aus, damit sich nichts ändert.
            enabled: false
          },
          items: [
            {
              content: parameters.content,
              timeLimit: parameters.timeLimit !== undefined ? parameters.timeLimit : 10,
              scoreThreshold: 100
            }
          ],
          practice: {
            enabled: false
          },
          timing: {
            startMode: parameters.startMode || 'click',
            advanceMode: 'click',
            warningAt: parameters.warningAt !== undefined ? parameters.warningAt : 3,
            showBar: parameters.showBar !== undefined ? parameters.showBar : true,
            onTimeout: parameters.onTimeout || 'lock',
            timeFactor: 1
          },
          confidence: {
            enabled: !!oldConf.enabled,
            timing: oldConf.timing || 'after',
            question: oldConf.question,
            coverText: oldConf.coverText,
            fbConfCorrect: oldConf.fbConfCorrect,
            fbConfWrong: oldConf.fbConfWrong,
            fbUnsureCorrect: oldConf.fbUnsureCorrect,
            fbUnsureWrong: oldConf.fbUnsureWrong
            // fbMidCorrect/fbMidWrong: neue Felder, Defaults aus semantics greifen.
          },
          classification: {
            // 1 Item, altes Verhalten: nur volle Punktzahl in der Zeit = Experte.
            expertMin: 100,
            intermediateMin: 40
          },
          paths: {
            expertUrl: parameters.expertUrl,
            expertLabel: parameters.expertLinkLabel,
            noviceUrl: parameters.noviceUrl,
            noviceLabel: parameters.noviceLinkLabel,
            redirectMode: parameters.redirectMode || 'link',
            redirectDelay: parameters.redirectDelay !== undefined ? parameters.redirectDelay : 4
          },
          behaviour: {
            // Alter Retry-Button gesetzt -> Wiederholung bleibt erlaubt.
            lockAfterCompletion: !parameters.retryLabel,
            startLabel: parameters.startLabel,
            timeoutMessage: parameters.timeoutMessage,
            expertMessage: parameters.successMessage
          }
        };

        finished(null, newParams, extras);
      }
    }
  };
})();
