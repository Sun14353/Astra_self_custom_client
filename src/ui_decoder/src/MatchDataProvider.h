/**
 * @file MatchDataProvider.h
 * @brief 赛场信息下行数据源 (QObject)，供 QML HUD 绑定。
 *
 * 通过 Paho MQTT 订阅协议中自定义客户端下行的 topic (GameStatus /
 * RobotDynamicStatus / RobotStaticStatus / GlobalUnitStatus / RobotPosition /
 * Event)，用 protobuf 反序列化后暴露为 Q_PROPERTY，供 QML 界面实时显示。
 *
 * 线程模型：MQTT 接收在后台线程，解析结果通过 QMetaObject::invokeMethod
 * (QueuedConnection) 切回 Qt 主线程再更新属性并 emit notify，避免跨线程竞争。
 */

#ifndef UIDECODER_MATCHDATAPROVIDER_H
#define UIDECODER_MATCHDATAPROVIDER_H

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class MyMqttClient;
class QTimer;

class MatchDataProvider : public QObject {
  Q_OBJECT

  // --- 连接状态 ---
  Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

  // --- GameStatus ---
  Q_PROPERTY(int redScore READ redScore NOTIFY gameStatusChanged)
  Q_PROPERTY(int blueScore READ blueScore NOTIFY gameStatusChanged)
  Q_PROPERTY(int currentRound READ currentRound NOTIFY gameStatusChanged)
  Q_PROPERTY(int totalRounds READ totalRounds NOTIFY gameStatusChanged)
  Q_PROPERTY(int stage READ stage NOTIFY gameStatusChanged)
  Q_PROPERTY(int stageCountdownSec READ stageCountdownSec NOTIFY gameStatusChanged)
  Q_PROPERTY(int stageElapsedSec READ stageElapsedSec NOTIFY gameStatusChanged)
  Q_PROPERTY(bool paused READ paused NOTIFY gameStatusChanged)
  Q_PROPERTY(QString gamePhase READ gamePhase NOTIFY gameStatusChanged)

  // --- RobotStaticStatus ---
  Q_PROPERTY(int robotId READ robotId NOTIFY robotStaticChanged)
  Q_PROPERTY(int robotType READ robotType NOTIFY robotStaticChanged)
  Q_PROPERTY(int maxHealth READ maxHealth NOTIFY robotStaticChanged)
  Q_PROPERTY(int maxHeat READ maxHeat NOTIFY robotStaticChanged)

  // --- RobotDynamicStatus ---
  Q_PROPERTY(int currentHealth READ currentHealth NOTIFY robotDynamicChanged)
  Q_PROPERTY(float currentHeat READ currentHeat NOTIFY robotDynamicChanged)
  Q_PROPERTY(int remainingAmmo READ remainingAmmo NOTIFY robotDynamicChanged)
  Q_PROPERTY(int chassisEnergy READ chassisEnergy NOTIFY robotDynamicChanged)
  Q_PROPERTY(int bufferEnergy READ bufferEnergy NOTIFY robotDynamicChanged)
  Q_PROPERTY(int currentExperience READ currentExperience NOTIFY robotDynamicChanged)
  Q_PROPERTY(int experienceForUpgrade READ experienceForUpgrade NOTIFY robotDynamicChanged)

  // --- GlobalUnitStatus ---
  Q_PROPERTY(int baseHealth READ baseHealth NOTIFY globalUnitChanged)
  Q_PROPERTY(int outpostHealth READ outpostHealth NOTIFY globalUnitChanged)
  Q_PROPERTY(int enemyBaseHealth READ enemyBaseHealth NOTIFY globalUnitChanged)
  Q_PROPERTY(int enemyOutpostHealth READ enemyOutpostHealth NOTIFY globalUnitChanged)
  Q_PROPERTY(int allyDamage READ allyDamage NOTIFY globalUnitChanged)
  Q_PROPERTY(int enemyDamage READ enemyDamage NOTIFY globalUnitChanged)

  // --- RobotPosition ---
  Q_PROPERTY(float posX READ posX NOTIFY positionChanged)
  Q_PROPERTY(float posY READ posY NOTIFY positionChanged)
  Q_PROPERTY(float posYaw READ posYaw NOTIFY positionChanged)

  // --- Event ---
  Q_PROPERTY(int lastEventId READ lastEventId NOTIFY eventChanged)
  Q_PROPERTY(QString lastEventText READ lastEventText NOTIFY eventChanged)

public:
  explicit MatchDataProvider(QObject *parent = nullptr);
  ~MatchDataProvider() override;

  /// 连接 MQTT broker 并订阅下行 topic（可重复调用，内部幂等/自动重连）。
  void start(const QString &brokerIp, int port, quint32 robotId);
  void stop();

  bool connected() const { return m_connected; }

  int redScore() const { return m_redScore; }
  int blueScore() const { return m_blueScore; }
  int currentRound() const { return m_currentRound; }
  int totalRounds() const { return m_totalRounds; }
  int stage() const { return m_stage; }
  int stageCountdownSec() const { return m_stageCountdownSec; }
  int stageElapsedSec() const { return m_stageElapsedSec; }
  bool paused() const { return m_paused; }
  QString gamePhase() const { return m_gamePhase; }

  int robotId() const { return m_robotIdValue; }
  int robotType() const { return m_robotType; }
  int maxHealth() const { return m_maxHealth; }
  int maxHeat() const { return m_maxHeat; }

  int currentHealth() const { return m_currentHealth; }
  float currentHeat() const { return m_currentHeat; }
  int remainingAmmo() const { return m_remainingAmmo; }
  int chassisEnergy() const { return m_chassisEnergy; }
  int bufferEnergy() const { return m_bufferEnergy; }
  int currentExperience() const { return m_currentExperience; }
  int experienceForUpgrade() const { return m_experienceForUpgrade; }

  int baseHealth() const { return m_baseHealth; }
  int outpostHealth() const { return m_outpostHealth; }
  int enemyBaseHealth() const { return m_enemyBaseHealth; }
  int enemyOutpostHealth() const { return m_enemyOutpostHealth; }
  int allyDamage() const { return m_allyDamage; }
  int enemyDamage() const { return m_enemyDamage; }

  float posX() const { return m_posX; }
  float posY() const { return m_posY; }
  float posYaw() const { return m_posYaw; }

  int lastEventId() const { return m_lastEventId; }
  QString lastEventText() const { return m_lastEventText; }

  /// 将 stage 数值映射为可读阶段文本（供 QML 调用）。
  Q_INVOKABLE QString stageName(int stageValue) const;
  /// 将 event_id 映射为可读事件文本（供 QML 调用）。
  Q_INVOKABLE QString eventName(int eventId) const;

signals:
  void connectedChanged();
  void gameStatusChanged();
  void robotStaticChanged();
  void robotDynamicChanged();
  void globalUnitChanged();
  void positionChanged();
  void eventChanged();

private:
  void onMqttMessage(const std::string &topic, const std::vector<uint8_t> &payload);
  void tryConnect();
  void updateConnected();

  std::unique_ptr<MyMqttClient> m_mqtt;
  QTimer *m_connTimer = nullptr;

  QString m_brokerIp;
  int m_port = 3333;
  quint32 m_robotId = 7;

  bool m_connected = false;

  int m_redScore = 0;
  int m_blueScore = 0;
  int m_currentRound = 1;
  int m_totalRounds = 0;
  int m_stage = 0;
  int m_stageCountdownSec = 0;
  int m_stageElapsedSec = 0;
  bool m_paused = false;
  QString m_gamePhase;

  int m_robotIdValue = 0;
  int m_robotType = 0;
  int m_maxHealth = 0;
  int m_maxHeat = 0;

  int m_currentHealth = 0;
  float m_currentHeat = 0.0f;
  int m_remainingAmmo = 0;
  int m_chassisEnergy = 0;
  int m_bufferEnergy = 0;
  int m_currentExperience = 0;
  int m_experienceForUpgrade = 0;

  int m_baseHealth = 0;
  int m_outpostHealth = 0;
  int m_enemyBaseHealth = 0;
  int m_enemyOutpostHealth = 0;
  int m_allyDamage = 0;
  int m_enemyDamage = 0;

  float m_posX = 0.0f;
  float m_posY = 0.0f;
  float m_posYaw = 0.0f;

  int m_lastEventId = 0;
  QString m_lastEventText;
};

#endif // UIDECODER_MATCHDATAPROVIDER_H
