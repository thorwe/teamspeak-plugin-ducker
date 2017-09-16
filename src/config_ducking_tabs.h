#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QRadioButton>

#include "volume_widgets/fader_vertical.h"

class Config_Ducking_Tabs : public QDialog
{
    Q_OBJECT
    
public:
    explicit Config_Ducking_Tabs(QWidget *parent = 0);

    // For Settings initialization and updating from other sources of interaction
    void UpdateChannelDuckerEnabled(bool);
    void UpdateChannelDuckerValue(float);
    void UpdateChannelDuckerReverse(bool);
    void UpdateChannelDuckerDuckPSEnabled(bool);

signals:
    void channelDuckerEnabledSet(bool);
    void channelDuckerValueChanged(float);
    void channelDuckerReverseSet(bool);
    void channelDuckerDuckPSEnabledSet(bool);

private slots:
    void onCRadioTargetCurrentToggled(bool val);
    void onCRadioTargetOtherToggled(bool val);

private:
    QPushButton* m_enabled_button;
    FaderVertical* m_fader;

    QRadioButton* m_radio_target_current;
    QRadioButton* m_radio_target_other;
    QCheckBox* m_checkbox_priority_speaker;
};
