#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>

#include "volume_widgets/fader_vertical.h"

class Config_Ducking_Client : public QDialog
{
    Q_OBJECT
    
public:
    explicit Config_Ducking_Client(QWidget *parent = 0);

    // For Settings initialization and updating from other sources of interaction
    void UpdateGlobalDuckerEnabled(bool);
    void UpdateGlobalDuckerValue(float);

signals:
    void globalDuckerEnabledSet(bool);
    void globalDuckerValueChanged(float);

private:
    QPushButton* m_enabled_button;
    FaderVertical* m_fader;
};
