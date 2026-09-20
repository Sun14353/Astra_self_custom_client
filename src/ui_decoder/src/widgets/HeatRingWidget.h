#ifndef UIDECODER_HEATRINGWIDGET_H
#define UIDECODER_HEATRINGWIDGET_H

#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QTimer>
#include <QWidget>

namespace ui_decoder {

class HeatRingWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(float shakeOffset READ getShakeOffset WRITE setShakeOffset)
  Q_PROPERTY(float flashOpacity READ getFlashOpacity WRITE setFlashOpacity)
  Q_PROPERTY(float pulseScale READ getPulseScale WRITE setPulseScale)
  Q_PROPERTY(float warningIntensity READ getWarningIntensity WRITE setWarningIntensity)

public:
  enum HeatLevel { Normal, LowWarning, MidWarning, HighWarning, Overheat };

  explicit HeatRingWidget(QWidget *parent = nullptr);

  void setHeat(int current, int maximum);
  void setDualBarrel(bool dual);
  void setBarrel1Heat(int current, int maximum);
  void setBarrel2Heat(int current, int maximum);
  void setRingSize(int size);
  void setThickness(int thickness);

  void setHeatThresholds(float lowWarning = 0.5f, float midWarning = 0.75f,
                         float highWarning = 0.9f);
  void setOverheatPenalty(bool enabled);
  void setHeatDecayRate(float rate);

  void setGlowEnabled(bool enabled);
  void setPulseEnabled(bool enabled);
  void setShakeEnabled(bool enabled);
  void setParticleEnabled(bool enabled);
  void setWarningFlashEnabled(bool enabled);
  void setBreathingEnabled(bool enabled);

  bool isGlowEnabled() const { return m_glowEnabled; }
  bool isPulseEnabled() const { return m_pulseEnabled; }
  bool isShakeEnabled() const { return m_shakeEnabled; }
  bool isParticleEnabled() const { return m_particleEnabled; }
  bool isWarningFlashEnabled() const { return m_warningFlashEnabled; }
  bool isBreathingEnabled() const { return m_breathingEnabled; }

  float getShakeOffset() const { return m_shakeOffset; }
  void setShakeOffset(float offset);
  float getFlashOpacity() const { return m_flashOpacity; }
  void setFlashOpacity(float opacity);
  float getPulseScale() const { return m_pulseScale; }
  void setPulseScale(float scale);
  float getWarningIntensity() const { return m_warningIntensity; }
  void setWarningIntensity(float intensity);

  int getCurrentHeat() const { return m_currentHeat; }
  int getMaxHeat() const { return m_maxHeat; }
  bool isDualBarrel() const { return m_dualBarrel; }
  HeatLevel getHeatLevel() const;
  bool isOverheated() const;

protected:
  void paintEvent(QPaintEvent *event) override;

public slots:
  void onAnimationTimer();
  void triggerOverheatShake();
  void triggerWarningFlash();
  void triggerOverheatPenalty();

private slots:
  void updateShake();
  void updateFlash();
  void updatePulse();
  void updateWarning();

private:
  int m_currentHeat;
  int m_maxHeat;
  bool m_dualBarrel;

  int m_barrel1Current;
  int m_barrel1Max;
  int m_barrel2Current;
  int m_barrel2Max;

  int m_ringSize;
  int m_thickness;

  float m_lowWarningThreshold;
  float m_midWarningThreshold;
  float m_highWarningThreshold;
  bool m_overheatPenaltyEnabled;
  float m_heatDecayRate;

  QTimer *m_animationTimer;
  float m_animationPhase;

  bool m_glowEnabled;
  bool m_pulseEnabled;
  bool m_shakeEnabled;
  bool m_particleEnabled;
  bool m_warningFlashEnabled;
  bool m_breathingEnabled;

  QPropertyAnimation *m_shakeAnimation;
  QPropertyAnimation *m_flashAnimation;
  QPropertyAnimation *m_pulseAnimation;
  QPropertyAnimation *m_warningAnimation;
  QSequentialAnimationGroup *m_overheatSequence;
  QGraphicsOpacityEffect *m_opacityEffect;
  QGraphicsDropShadowEffect *m_glowEffect;

  float m_shakeOffset;
  float m_flashOpacity;
  float m_pulseScale;
  float m_warningIntensity;
  QList<QPointF> m_particles;
  QList<float> m_particleLife;
  QList<QPointF> m_particleVelocity;

  HeatLevel m_lastLevel;

  void setupAnimation();
  void drawSingleBarrelRing(QPainter &painter);
  void drawDualBarrelRings(QPainter &painter);
  void drawHeatRing(QPainter &painter, int current, int maximum,
                    const QPointF &center, int radius, int thickness);
  void drawParticles(QPainter &painter);
  void drawWarningIndicators(QPainter &painter, HeatLevel level);
  void drawOverheatPenalty(QPainter &painter);
  void drawHeatDecayIndicator(QPainter &painter);
  void setupAnimations();
  void applyGlowEffect();
  void applyShakeEffect();

  QColor getHeatColor(int current, int maximum) const;
  QColor getEnhancedHeatColor(int current, int maximum) const;
  HeatLevel calculateHeatLevel(int current, int maximum) const;
  float getHeatPercentage(int current, int maximum) const;
  void updateParticleSystem();
};

} // namespace ui_decoder

#endif // UIDECODER_HEATRINGWIDGET_H
