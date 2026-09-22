// ============================================================
// todo_login / player_profile_model.h
// 架构骨架：选手档案模型。
//
// TODO:
//   - 扫描 todo_presets/presets/players/*.yaml|json，填充选手列表
//   - 选定后通知 todo_control 加载对应预设
// ============================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class PlayerProfileModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString selectedPlayer READ selectedPlayer
                 WRITE setSelectedPlayer NOTIFY selectedPlayerChanged)

public:
  explicit PlayerProfileModel(QObject* parent = nullptr);

  QString selectedPlayer() const { return m_selected; }
  void setSelectedPlayer(const QString& player);

  // TODO: 返回可用选手/预设 id 列表（当前为空占位）。
  Q_INVOKABLE QStringList availablePlayers() const { return m_available; }

signals:
  void selectedPlayerChanged();

private:
  QString m_selected;
  QStringList m_available;  // TODO: 从 todo_presets 扫描填充
};
