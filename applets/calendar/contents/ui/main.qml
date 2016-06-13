/*
 * Copyright 2013  Heena Mahour <heena393@gmail.com>
 * Copyright 2013 Sebastian Kügler <sebas@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
import QtQuick 2.0
import QtQuick.Layouts 1.1

import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore

import org.kde.plasma.calendar 2.0

Item {
    Plasmoid.switchWidth: units.gridUnit * 12
    Plasmoid.switchHeight: units.gridUnit * 12

    Layout.minimumWidth: units.iconSizes.large
    Layout.minimumHeight: units.iconSizes.large

    Plasmoid.fullRepresentation: Item {

        // sizing taken from digital clock
        readonly property int _minimumWidth: calendar.showWeekNumbers ? Math.round(_minimumHeight * 1.75) : Math.round(_minimumHeight * 1.5)
        readonly property int _minimumHeight: units.gridUnit * 14

        Layout.minimumWidth: _minimumWidth
        Layout.minimumHeight: _minimumHeight
        Layout.preferredWidth: _minimumWidth
        Layout.preferredHeight: Math.round(_minimumHeight * 1.5)

        PlasmaCore.DataSource {
            id: dataSource
            engine: "time"
            connectedSources: ["Local"]
            interval: 60000
            intervalAlignment: PlasmaCore.Types.AlignToMinute
        }

        MonthView {
            id: calendar
            today: dataSource.data["Local"]["DateTime"]
            showWeekNumbers: plasmoid.configuration.showWeekNumbers

            anchors.fill: parent
        }
    }
}
