#ifdef SHOUTOUT_KDE_SMOKE
#include <KCModule>
#include <KPluginFactory>
#else
#include "settingspanel.h"
#endif
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <cstdio>
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  #ifdef SHOUTOUT_KDE_SMOKE
  if (argc < 2)
    return 2;
  auto result = KPluginFactory::instantiatePlugin<KCModule>(
      KPluginMetaData(QString::fromLocal8Bit(argv[1])), nullptr);
  if (!result) {
    fprintf(stderr, "%s\n", qPrintable(result.errorText));
    return 1;
  }
  auto module = result.plugin;
  auto w = module->widget();
  #else
  auto module = new SettingsPanel;
  auto w = module;
  #endif
  w->resize(780, 660);
  w->show();
  module->load();
  QTimer::singleShot(7000, w, [&] {
    auto address = w->findChild<QLineEdit *>("receiverAddress");
    if (!address || !address->text().contains(':') ||
        w->findChild<QSpinBox *>("receiverPort")) {
      app.exit(1);
      return;
    }
    auto scale = w->findChild<QDoubleSpinBox *>("volumeScale");
    auto devices = w->findChild<QComboBox *>("destination");
    auto status = w->findChild<QLabel *>("status");
    auto codec = w->findChild<QComboBox *>("codec");
    auto segment = w->findChild<QSpinBox *>("segmentMS");
    if (w->findChild<QPushButton *>("discover") || !codec ||
        codec->count() != 2 || !segment || segment->minimum() != 250 ||
        segment->maximum() != 2000 || !scale || scale->maximum() != 100 ||
        !devices || devices->count() < 2 || devices->width() < 300 || !status ||
        status->text().isEmpty()) {
      fprintf(stderr, "Native controls or discovery failed\n");
      app.exit(1);
      return;
    }
    int previousWidth = devices->width();
    w->resize(w->width() + 240, w->height());
    app.processEvents();
    if (devices->width() < previousWidth + 100) {
      fprintf(stderr, "Destination field does not expand with the page\n");
      app.exit(1);
      return;
    }
    auto sliders = w->findChildren<QSlider *>();
    auto volumeSlider = w->findChild<QSlider *>("volumeScaleSlider");
    if (sliders.size() != 3 || !volumeSlider) {
      fprintf(stderr, "Missing native sliders\n");
      app.exit(1);
      return;
    }
    auto preset = w->findChild<QComboBox *>("preset");
    auto delay = w->findChild<QSpinBox *>("targetDelayMS");
    auto bitrate = w->findChild<QComboBox *>("bitrate");
    if (!preset || preset->count() != 4 || !delay || !bitrate || delay->minimum() != 10 ||
        delay->singleStep() != 10) {
      app.exit(1);
      return;
    }
    for (int i = 0; i < 3; ++i) {
      preset->setCurrentIndex(i);
      QMetaObject::invokeMethod(preset, "activated", Qt::DirectConnection,
                                Q_ARG(int, i));
      if (codec->currentData().toString() !=
              (i == 2 ? "aac-hls" : "cast-opus") ||
          bitrate->currentData().toInt() != (i == 0   ? 128
                                             : i == 1 ? 192
                                                      : 320) ||
          (i < 2 && delay->value() != (i == 0 ? 20 : 100)) ||
          (i == 2 && segment->value() != 500)) {
        fprintf(stderr, "Preset mapping failed\n");
        app.exit(1);
        return;
      }
    }
    double previous = scale->value();
    volumeSlider->setValue(31);
    if (qAbs(scale->value() - 3.1) > 0.001) {
      app.exit(1);
      return;
    }
    scale->setValue(2.7);
    if (volumeSlider->value() != 27) {
      app.exit(1);
      return;
    }
    scale->setValue(previous);
    printf("Native settings loaded; full 0–100%% scale; %d destination "
           "entries; status available.\n",
           devices->count());
    if (argc > 2)
      w->grab().save(QString::fromLocal8Bit(argv[2]));
    app.exit(0);
  });
  return app.exec();
}
