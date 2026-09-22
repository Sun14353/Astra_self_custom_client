#include "MatchDataProvider.h"

#include "network/MyMqttClient.h"
#include "custom_client.pb.h"

#include <QMetaObject>
#include <QTimer>

#include <string>
#include <utility>

namespace {
constexpr int kReconnectIntervalMs = 2000;
} // namespace

MatchDataProvider::MatchDataProvider(QObject *parent)
    : QObject(parent) {
  m_connTimer = new QTimer(this);
  m_connTimer->setInterval(kReconnectIntervalMs);
  connect(m_connTimer, &QTimer::timeout, this, [this]() {
    tryConnect();
    updateConnected();
  });
}

MatchDataProvider::~MatchDataProvider() {
  stop();
}

void MatchDataProvider::start(const QString &brokerIp, int port, quint32 robotId) {
  m_brokerIp = brokerIp;
  m_port = port;
  m_robotId = robotId;

  m_mqtt = std::make_unique<MyMqttClient>(robotId, brokerIp.toStdString(), port);
  m_mqtt->SetProtoHandler([this](const std::string &topic,
                                 const std::vector<uint8_t> &payload) {
    onMqttMessage(topic, payload);
  });

  tryConnect();
  m_connTimer->start();
}

void MatchDataProvider::stop() {
  if (m_connTimer) {
    m_connTimer->stop();
  }
  if (m_mqtt) {
    m_mqtt->StopReceiveLoop();
    m_mqtt->Disconnect();
    m_mqtt.reset();
  }
}

void MatchDataProvider::tryConnect() {
  if (!m_mqtt) {
    return;
  }

  if (!m_mqtt->IsConnected()) {
    if (m_mqtt->Connect()) {
      m_mqtt->StartReceiveLoop();
    }
  }

  if (m_mqtt->IsConnected()) {
    // 幂等：Subscribe 内部会记录，重复调用无害；重连后也会自动重订阅。
    m_mqtt->Subscribe("GameStatus");
    m_mqtt->Subscribe("GlobalUnitStatus");
    m_mqtt->Subscribe("RobotStaticStatus");
    m_mqtt->Subscribe("RobotDynamicStatus");
    m_mqtt->Subscribe("RobotPosition");
    m_mqtt->Subscribe("Event");
    m_mqtt->Subscribe("PenaltyInfo");
  }
}

void MatchDataProvider::updateConnected() {
  const bool connected = m_mqtt && m_mqtt->IsConnected();
  if (connected != m_connected) {
    m_connected = connected;
    emit connectedChanged();
  }
}

QString MatchDataProvider::stageName(int stageValue) const {
  switch (stageValue) {
  case 3:
    return QStringLiteral("准备阶段");
  case 4:
    return QStringLiteral("战斗阶段");
  case 5:
    return QStringLiteral("结算阶段");
  default:
    return QStringLiteral("阶段 %1").arg(stageValue);
  }
}

QString MatchDataProvider::eventName(int eventId) const {
  switch (eventId) {
  case 1:
    return QStringLiteral("击毁对方步兵");
  case 2:
    return QStringLiteral("对方前哨站被摧毁");
  case 4:
    return QStringLiteral("己方激活小能量机关");
  case 5:
    return QStringLiteral("己方英雄完成狙击");
  case 9:
    return QStringLiteral("飞镖命中对方前哨站");
  case 11:
    return QStringLiteral("己方基地遭到攻击");
  default:
    return QStringLiteral("事件 %1").arg(eventId);
  }
}

void MatchDataProvider::onMqttMessage(const std::string &topic,
                                      const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }

  // 赛场信息下行 topic 与 RoboMaster 2026 官方协议一致。
  if (topic == "GameStatus") {
    custom_client::GameStatus m;
    if (m.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
      const int red = m.red_score();
      const int blue = m.blue_score();
      const int round = m.current_round();
      const int total = m.total_rounds();
      const int stage = m.current_stage();
      const int countdown = m.stage_countdown_sec();
      const int elapsed = m.stage_elapsed_sec();
      const bool paused = m.is_paused();
      QMetaObject::invokeMethod(this, [this, red, blue, round, total, stage,
                                      countdown, elapsed, paused]() {
        m_redScore = red;
        m_blueScore = blue;
        m_currentRound = round;
        m_totalRounds = total;
        m_stage = stage;
        m_stageCountdownSec = countdown;
        m_stageElapsedSec = elapsed;
        m_paused = paused;
        m_gamePhase = paused ? QStringLiteral("比赛暂停") : stageName(stage);
        emit gameStatusChanged();
      }, Qt::QueuedConnection);
    }
    return;
  }

  if (topic == "RobotStaticStatus") {
    custom_client::RobotStaticStatus m;
    if (m.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
      const int rid = m.robot_id();
      const int rtype = m.robot_type();
      const int maxHp = m.max_health();
      const int maxHeat = m.max_heat();
      QMetaObject::invokeMethod(this, [this, rid, rtype, maxHp, maxHeat]() {
        m_robotIdValue = rid;
        m_robotType = rtype;
        m_maxHealth = maxHp;
        m_maxHeat = maxHeat;
        emit robotStaticChanged();
      }, Qt::QueuedConnection);
    }
    return;
  }

  if (topic == "RobotDynamicStatus") {
    custom_client::RobotDynamicStatus m;
    if (m.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
      const int hp = m.current_health();
      const float heat = m.current_heat();
      const int ammo = m.remaining_ammo();
      const int chassis = m.current_chassis_energy();
      const int buffer = m.current_buffer_energy();
      const int exp = m.current_experience();
      const int expUp = m.experience_for_upgrade();
      QMetaObject::invokeMethod(this, [this, hp, heat, ammo, chassis, buffer,
                                      exp, expUp]() {
        m_currentHealth = hp;
        m_currentHeat = heat;
        m_remainingAmmo = ammo;
        m_chassisEnergy = chassis;
        m_bufferEnergy = buffer;
        m_currentExperience = exp;
        m_experienceForUpgrade = expUp;
        emit robotDynamicChanged();
      }, Qt::QueuedConnection);
    }
    return;
  }

  if (topic == "GlobalUnitStatus") {
    custom_client::GlobalUnitStatus m;
    if (m.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
      const int base = m.base_health();
      const int outpost = m.outpost_health();
      const int ebase = m.enemy_base_health();
      const int eoutpost = m.enemy_outpost_health();
      const int ally = m.total_damage_ally();
      const int enemy = m.total_damage_enemy();
      QMetaObject::invokeMethod(this, [this, base, outpost, ebase, eoutpost,
                                      ally, enemy]() {
        m_baseHealth = base;
        m_outpostHealth = outpost;
        m_enemyBaseHealth = ebase;
        m_enemyOutpostHealth = eoutpost;
        m_allyDamage = ally;
        m_enemyDamage = enemy;
        emit globalUnitChanged();
      }, Qt::QueuedConnection);
    }
    return;
  }

  if (topic == "RobotPosition") {
    custom_client::RobotPosition m;
    if (m.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
      const float x = m.x();
      const float y = m.y();
      const float yaw = m.yaw();
      QMetaObject::invokeMethod(this, [this, x, y, yaw]() {
        m_posX = x;
        m_posY = y;
        m_posYaw = yaw;
        emit positionChanged();
      }, Qt::QueuedConnection);
    }
    return;
  }

  if (topic == "Event") {
    custom_client::Event m;
    if (m.ParseFromArray(payload.data(), static_cast<int>(payload.size()))) {
      const int id = m.event_id();
      const QString param = QString::fromStdString(m.param());
      QMetaObject::invokeMethod(this, [this, id, param]() {
        m_lastEventId = id;
        m_lastEventText = param.isEmpty() ? eventName(id)
                                          : QStringLiteral("%1 (%2)").arg(eventName(id), param);
        emit eventChanged();
      }, Qt::QueuedConnection);
    }
    return;
  }

  // 其余 topic (如 PenaltyInfo / status/robot) 暂不展示，静默忽略。
}
