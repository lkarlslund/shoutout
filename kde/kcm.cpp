#include "settingspanel.h"
#include <KCModule>
#include <KPluginFactory>
#include <QVBoxLayout>

class ShoutoutKCM : public KCModule {
  Q_OBJECT
public:
  ShoutoutKCM(QObject *parent, const KPluginMetaData &data)
      : KCModule(parent, data), panel(new SettingsPanel(widget())) {
    setButtons(Apply | Default);
    auto layout = new QVBoxLayout(widget());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(panel);
    connect(panel, &SettingsPanel::changed, this,
            [this](bool dirty) { setNeedsSave(dirty); });
  }
  void load() override { panel->load(); }
  void save() override { panel->save(); }
  void defaults() override { panel->defaults(); }
private:
  SettingsPanel *panel;
};
K_PLUGIN_CLASS_WITH_JSON(ShoutoutKCM, "kcm_shoutout.json")
#include "kcm.moc"
