// Codec probe for Qt WebEngine: prints canPlayType / MSE answers for the
// formats x.com (H.264 + AAC in MSE) needs.
#include <QApplication>
#include <QTimer>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QWebEngineView view;
    QWebEnginePage *page = view.page();

    const QString js = QStringLiteral(R"JS(
      (function () {
        var out = [];
        var v = document.createElement('video');
        var probes = [
          'video/mp4; codecs="avc1.42E01E"',
          'video/mp4; codecs="avc1.64001F"',
          'video/mp4; codecs="mp4a.40.2"',
          'video/mp4',
          'audio/mp4; codecs="mp4a.40.2"',
          'video/webm; codecs="vp9"',
          'video/webm; codecs="vp8"',
          'audio/webm; codecs="vorbis"',
          'video/mp4; codecs="hev1.1.6.L93.B0"',
          'application/vnd.apple.mpegurl'
        ];
        probes.forEach(function (p) { out.push(p + ' => ' + (v.canPlayType(p) || 'no')); });
        try {
          out.push('MSE mp4/avc1 => ' +
            MediaSource.isTypeSupported('video/mp4; codecs="avc1.42E01E"'));
          out.push('MSE mp4/opus => ' +
            MediaSource.isTypeSupported('video/mp4; codecs="opus"'));
        } catch (e) { out.push('MSE => error ' + e); }
        out.push('UA => ' + navigator.userAgent);
        return out.join('\n');
      })()
    )JS");

    QObject::connect(page, &QWebEnginePage::loadFinished, [page, js](bool ok) {
        if (!ok) { fprintf(stdout, "load failed\n"); fflush(stdout); QCoreApplication::quit(); return; }
        page->runJavaScript(js, [](const QVariant &r) {
            fprintf(stdout, "%s\n", r.toString().toUtf8().constData());
            fflush(stdout);
            QCoreApplication::quit();
        });
    });

    QTimer::singleShot(25000, &app, [] { fprintf(stdout, "TIMEOUT\n"); fflush(stdout); QCoreApplication::quit(); });
    page->setUrl(QUrl("about:blank"));
    return app.exec();
}
