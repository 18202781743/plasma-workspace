/*
    SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

import QtQuick 2.6
import QtQuick.Layouts 1.3
import QtQuick.Controls as QQC2

import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.kcm.users 1.0 as UsersKCM

Kirigami.PromptDialog {
    id: passwordRoot

    preferredWidth: Kirigami.Units.gridUnit * 20

    function openAndClear() {
        verifyField.text = ""
        passwordField.text = ""
        passwordField.forceActiveFocus()
        open()
    }

    required property Kirigami.Page usersDetailPage
    property UsersKCM.User user

    title: i18n("Change Password")
    parent: usersDetailPage.QQC2.Overlay.overlay

    standardButtons: Kirigami.Dialog.Cancel
    customFooterActions: Kirigami.Action {
        id: passAction
        text: i18n("Set Password")
        enabled: !passwordWarning.visible && verifyField.text && passwordField.text
        onTriggered: {
            if (passwordField.text !== verifyField.text) {
                debouncer.isTriggered = true
                return
            }
            passwordRoot.user.setPassword(passwordField.text)
            passwordRoot.close()
        }
    }

    ColumnLayout {
        id: mainColumn
        spacing: Kirigami.Units.smallSpacing

        Kirigami.PasswordField {
            id: passwordField

            Layout.fillWidth: true

            placeholderText: i18n("Password")
            onTextChanged: debouncer.reset()

            onAccepted: {
                if (!passwordWarning.visible && verifyField.text && passwordField.text) {
                    passAction.trigger()
                }
            }
        }

        Kirigami.PasswordField {
            id: verifyField

            Layout.fillWidth: true

            placeholderText: i18n("Confirm password")
            onTextChanged: debouncer.reset()

            onAccepted: {
                if (!passwordWarning.visible && verifyField.text && passwordField.text) {
                    passAction.trigger()
                }
            }
        }

        Debouncer {
            id: debouncer
        }

        Kirigami.InlineMessage {
            id: passwordWarning

            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: i18n("Passwords must match")
            visible: passwordField.text !== ""
                && verifyField.text !== ""
                && passwordField.text !== verifyField.text
                && debouncer.isTriggered
        }
    }
}
