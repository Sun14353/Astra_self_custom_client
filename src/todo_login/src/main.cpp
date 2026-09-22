// ============================================================
// todo_login / main.cpp
// 架构骨架：选手登录选择界面入口。
//
// TODO:
//   - 启动顺序与参数注入（读取 todo_presets 目录）
//   - 与 todo_control 联动（选定选手后加载对应预设）
// ============================================================
#include <QApplication>
#include <QQmlContext>
#include <QQuickWidget>
#include <QSurfaceFormat>
#include <QUrl>

#include "player_profile_model.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  // QQuickWidget 透明背景需要 alpha 通道，需在 QApplication 之前设置。
  QSurfaceFormat format;
  format.setAlphaBufferSize(8);
  QSurfaceFormat::setDefaultFormat(format);

  Q_INIT_RESOURCE(qml);

  QQuickWidget view;
  view.setResizeMode(QQuickWidget::SizeRootObjectToView);
  view.setClearColor(Qt::transparent);

  // 选手档案模型（骨架）：TODO 扫描 todo_presets 填充列表。
  PlayerProfileModel model;
  view.rootContext()->setContextProperty("playerModel", &model);

  view.setSource(QUrl(QStringLiteral("qrc:/TodoLoginScreen.qml")));
  view.resize(640, 480);
  view.show();

  return app.exec();
}
