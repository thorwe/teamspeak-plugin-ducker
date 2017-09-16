#include "config_ducking_client.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>

Config_Ducking_Client::Config_Ducking_Client(QWidget *parent) :
    QDialog(parent)
{
    setAttribute( Qt::WA_DeleteOnClose );
    setWindowTitle(" ");
    setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto layout = new QVBoxLayout(this);

    auto label = new QLabel();
    label->setText("<strong>Client Ducking</strong>");
    layout->addWidget(label, 0, Qt::AlignCenter);

    auto line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    m_enabled_button = new QPushButton(tr("Enabled"));
    m_enabled_button->setCheckable(true);
    connect(m_enabled_button, &QPushButton::toggled, this, &Config_Ducking_Client::globalDuckerEnabledSet);
    layout->addWidget(m_enabled_button, 0, Qt::AlignCenter);
    
    m_fader = new FaderVertical();
    connect(m_fader, &FaderVertical::valueChanged, this, &Config_Ducking_Client::globalDuckerValueChanged);
    layout->addWidget(m_fader, 1, Qt::AlignCenter);
    layout->addStretch(1);

    this->setLayout(layout);
    setFixedWidth(sizeHint().width());
    show();
}

void Config_Ducking_Client::UpdateGlobalDuckerEnabled(bool val)
{
    m_enabled_button->blockSignals(true);
    m_enabled_button->setChecked(val);
    m_enabled_button->blockSignals(false);
}

void Config_Ducking_Client::UpdateGlobalDuckerValue(float val)
{
    m_fader->blockSignals(true);
    m_fader->setValue(val);
    m_fader->blockSignals(false);
}
