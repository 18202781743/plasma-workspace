/*
    SPDX-FileCopyrightText: 2014 Martin Yrjölä <martin.yrjola@gmail.com>

    SPDX-License-Identifier: MIT
*/

#include "keyboardlayoutplugin.h"
#include "keyboardlayout.h"
#include "virtualkeyboard.h"

#include <QDBusPendingReply>
#include <QQmlEngine>

// register type for Keyboards.KWinVirtualKeyboard.forceActivate();
Q_DECLARE_METATYPE(QDBusPendingReply<>)

void KeyboardLayoutPlugin::registerTypes(const char *uri)
{
    // register type for Keyboards.KWinVirtualKeyboard.forceActivate();
    qRegisterMetaType<QDBusPendingReply<>>();

    qmlRegisterType<KeyboardLayout>(uri, 1, 0, "KeyboardLayout");
    qmlRegisterSingletonType<KwinVirtualKeyboardInterface>(uri, 1, 0, "KWinVirtualKeyboard", [](QQmlEngine *, QJSEngine *) -> QObject * {
        return new KwinVirtualKeyboardInterface;
    });
}

#include "moc_keyboardlayoutplugin.cpp"
