// ============================================================
// todo_login / player_profile_model.cpp
// 架构骨架：选手档案模型实现。当前为空，TODO 实现。
// ============================================================
#include "player_profile_model.h"

PlayerProfileModel::PlayerProfileModel(QObject* parent) : QObject(parent) {
  // TODO: 扫描 todo_presets/presets/players 目录，填充 m_available。
}

void PlayerProfileModel::setSelectedPlayer(const QString& player) {
  if (player == m_selected) {
    return;
  }
  m_selected = player;
  emit selectedPlayerChanged();
  // TODO: 通知 todo_control 加载选定选手的预设（todo_presets）。
}
