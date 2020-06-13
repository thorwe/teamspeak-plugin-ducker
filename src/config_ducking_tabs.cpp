#include "config_ducking_tabs.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

Config_Ducking_Tabs::Config_Ducking_Tabs(QWidget *parent) :
    QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(" ");
    setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto layout = new QVBoxLayout();

    auto label = new QLabel();
    label->setText("<strong>Server Tab Ducking</strong>");
    layout->addWidget(label, 0, Qt::AlignCenter);

    auto line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    // Channel Ducking
    m_enabled_button = new QPushButton(tr("Enabled"));
    m_enabled_button->setCheckable(true);
    connect(m_enabled_button, &QPushButton::toggled, this, &Config_Ducking_Tabs::channelDuckerEnabledSet);
    layout->addWidget(m_enabled_button, 0, Qt::AlignCenter);

    auto cTargetLayout = new QGridLayout();
    auto text = QString("<strong>%1</strong>").arg(tr("Target:"));
    auto cTargetLabel = new QLabel(text);
    cTargetLayout->addWidget(cTargetLabel, 0, 0, Qt::AlignCenter);

    m_radio_target_current = new QRadioButton(qApp->translate("HotkeyDialog","Current Server"));
    connect(m_radio_target_current, &QRadioButton::toggled, this, &Config_Ducking_Tabs::onCRadioTargetCurrentToggled);
    cTargetLayout->addWidget(m_radio_target_current, 0, 1, Qt::AlignLeft);

    auto otherServers = qApp->translate("NotificationsSetup", "Other");
    otherServers.append(" ");
    otherServers.append(qApp->translate("ImprovedTabBar", "Server Tabs"));
    m_radio_target_other = new QRadioButton(otherServers);
    connect(m_radio_target_other, &QRadioButton::toggled, this, &Config_Ducking_Tabs::onCRadioTargetOtherToggled);
    cTargetLayout->addWidget(m_radio_target_other, 1, 1, Qt::AlignLeft);

    //cTargetLayout->setColumnStretch(0,1);
    //cTargetLayout->setColumnStretch(1,2);
    cTargetLayout->setColumnMinimumWidth(1, 130);
    layout->addLayout(cTargetLayout);

    m_fader = new FaderVertical();
    connect(m_fader, &FaderVertical::valueChanged, this, &Config_Ducking_Tabs::channelDuckerValueChanged);
    layout->addWidget(m_fader, 0, Qt::AlignCenter);

    m_checkbox_priority_speaker = new QCheckBox("Duck Priority Speakers");
    connect(m_checkbox_priority_speaker, &QCheckBox::toggled, this, &Config_Ducking_Tabs::channelDuckerDuckPSEnabledSet);
    layout->addWidget(m_checkbox_priority_speaker, 0, Qt::AlignCenter);

    setLayout(layout);
}

void Config_Ducking_Tabs::UpdateChannelDuckerEnabled(bool val)
{
    m_enabled_button->blockSignals(true);
    m_enabled_button->setChecked(val);
    m_enabled_button->blockSignals(false);
}

void Config_Ducking_Tabs::UpdateChannelDuckerValue(float val)
{
    m_fader->blockSignals(true);
    m_fader->setValue(val);
    m_fader->blockSignals(false);
}

void Config_Ducking_Tabs::UpdateChannelDuckerReverse(bool val)
{
    m_radio_target_other->blockSignals(true);
    m_radio_target_current->blockSignals(true);
    m_radio_target_other->setChecked(val);
    m_radio_target_current->setChecked(!val);
    m_radio_target_other->blockSignals(false);
    m_radio_target_current->blockSignals(false);
}

void Config_Ducking_Tabs::UpdateChannelDuckerDuckPSEnabled(bool val)
{
    m_checkbox_priority_speaker->blockSignals(true);
    m_checkbox_priority_speaker->setChecked(val);
    m_checkbox_priority_speaker->blockSignals(false);
}

void Config_Ducking_Tabs::onCRadioTargetCurrentToggled(bool val)
{
    m_radio_target_other->blockSignals(true);
    m_radio_target_other->setChecked(!val);
    m_radio_target_other->blockSignals(false);
    emit channelDuckerReverseSet(!val);
}

void Config_Ducking_Tabs::onCRadioTargetOtherToggled(bool val)
{
    m_radio_target_current->blockSignals(true);
    m_radio_target_current->setChecked(!val);
    m_radio_target_current->blockSignals(false);
    emit channelDuckerReverseSet(val);
}
