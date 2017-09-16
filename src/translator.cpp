#include "translator.h"

#include "core/ts_logging_qt.h"
#include "core/ts_helpers_qt.h"

Translator* Translator::m_Instance = 0;

Translator::Translator(){}

bool Translator::InitLocalization()
{
    const auto kLang = TSHelpers::GetLanguage();
    if (kLang.isEmpty())
        return false;

    translator = new QTranslator(this);
    const auto kIsTranslate = translator->load(QStringLiteral(":/locales/ducker_") + kLang);
    if (!kIsTranslate)
        TSLogging::Log("No translation available.");

    return kIsTranslate;
}
