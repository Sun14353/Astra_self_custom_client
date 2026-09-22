import QtQuick 2.15

/**
 * @file MatchHud.qml
 * @brief 赛场信息 HUD 覆盖层 (仿照 EGA_Client TopInfoBar / 状态面板)
 *
 * 透明背景，叠加在视频画面上。数据来自 C++ 侧注入的 matchData
 * (MatchDataProvider)，展示比分 / 阶段倒计时 / 基地与机器人状态 / 事件。
 */

Item {
    id: root

    // 统一从 C++ 注入的上下文读取数据
    property var md: matchData

    // 避免机器人静态状态未到时除零
    property int hpMax: (md && md.maxHealth > 0) ? md.maxHealth : 600
    property int heatMax: (md && md.maxHeat > 0) ? md.maxHeat : 240

    function fmtTime(sec) {
        var s = Math.max(0, sec)
        var m = Math.floor(s / 60)
        var ss = s % 60
        return (m < 10 ? "0" + m : m) + ":" + (ss < 10 ? "0" + ss : ss)
    }

    function fmtCoord(v) {
        return v.toFixed(2)
    }

    // ==================== 顶部中央：阶段 / 时间 / 比分 ====================
    Rectangle {
        id: centerPanel
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        width: 320
        height: 92
        color: "#0E1113"
        opacity: 0.82
        radius: 6
        border.color: "#2A3138"
        border.width: 1

        Text {
            anchors.top: parent.top
            anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter
            text: md.paused ? "比赛暂停" : (md.gamePhase + " · Round " + md.currentRound)
            color: md.paused ? "#FFD54F" : "#66CCFF"
            font.pixelSize: 12
            font.bold: true
            font.family: "Arial"
        }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 10
            text: fmtTime(md.stageCountdownSec)
            color: md.paused ? "#FFD54F" : "#FF3333"
            font.pixelSize: 30
            font.bold: true
            font.family: "Arial"
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 64
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 10
            text: md.redScore
            color: "#FF5C5C"
            font.pixelSize: 22
            font.bold: true
            font.family: "Arial"
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 64
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 10
            text: md.blueScore
            color: "#5CA8FF"
            font.pixelSize: 22
            font.bold: true
            font.family: "Arial"
        }
    }

    // ==================== 左上：我方基地 / 前哨站 ====================
    Rectangle {
        id: allyPanel
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 8
        width: 240
        height: 74
        color: "#0E1113"
        opacity: 0.82
        radius: 6
        border.color: "#2A3138"

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Text { text: "我方基地"; color: "#FF6B6B"; font.pixelSize: 11; font.bold: true }
            Rectangle {
                width: parent.width
                height: 10
                radius: 5
                color: "#24292F"
                Rectangle {
                    height: parent.height
                    radius: 5
                    width: parent.width * Math.min(1, md.baseHealth / 5000)
                    color: "#FF5C5C"
                }
            }
            Text { text: "前哨站 " + md.outpostHealth + "/1500"; color: "#9AA4AD"; font.pixelSize: 11 }
        }
    }

    // ==================== 右上：敌方基地 / 前哨站 ====================
    Rectangle {
        id: enemyPanel
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        width: 240
        height: 74
        color: "#0E1113"
        opacity: 0.82
        radius: 6
        border.color: "#2A3138"

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Text { text: "敌方基地"; color: "#6BA5FF"; font.pixelSize: 11; font.bold: true }
            Rectangle {
                width: parent.width
                height: 10
                radius: 5
                color: "#24292F"
                Rectangle {
                    height: parent.height
                    radius: 5
                    width: parent.width * Math.min(1, md.enemyBaseHealth / 5000)
                    color: "#5CA8FF"
                }
            }
            Text { text: "前哨站 " + md.enemyOutpostHealth + "/1500"; color: "#9AA4AD"; font.pixelSize: 11 }
        }
    }

    // ==================== 左下：机器人状态 ====================
    Rectangle {
        id: robotPanel
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 8
        width: 240
        height: 148
        color: "#0E1113"
        opacity: 0.82
        radius: 6
        border.color: "#2A3138"

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 7

            Row {
                spacing: 8
                Text { text: "ROBOT " + md.robotId; color: "#00CED1"; font.pixelSize: 12; font.bold: true; font.family: "Consolas" }
                Text { text: md.connected ? "● ONLINE" : "○ OFFLINE"; color: md.connected ? "#00FF00" : "#FF4444"; font.pixelSize: 11; font.family: "Consolas" }
            }

            // 血量
            Text { text: "HP " + md.currentHealth + "/" + hpMax; color: "#EEF3F6"; font.pixelSize: 12 }
            Rectangle {
                width: parent.width
                height: 8
                radius: 4
                color: "#24292F"
                Rectangle {
                    height: parent.height
                    radius: 4
                    width: parent.width * Math.max(0, Math.min(1, md.currentHealth / hpMax))
                    color: "#33CC66"
                }
            }

            // 热量 (与中心热量环同源)
            Text { text: "HEAT " + md.currentHeat.toFixed(0) + "/" + heatMax; color: "#F5E37B"; font.pixelSize: 12 }

            // 弹药 / 能量
            Row {
                spacing: 16
                Text { text: "弹药 " + md.remainingAmmo; color: "#9AA4AD"; font.pixelSize: 11 }
                Text { text: "底盘 " + md.chassisEnergy; color: "#9AA4AD"; font.pixelSize: 11 }
                Text { text: "缓冲 " + md.bufferEnergy; color: "#9AA4AD"; font.pixelSize: 11 }
            }
            Text { text: "经验 " + md.currentExperience + "/" + md.experienceForUpgrade; color: "#9AA4AD"; font.pixelSize: 11 }
        }
    }

    // ==================== 右下：位置 / 伤害 ====================
    Rectangle {
        id: posPanel
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        width: 220
        height: 96
        color: "#0E1113"
        opacity: 0.82
        radius: 6
        border.color: "#2A3138"

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            Text { text: "POSITION"; color: "#00CED1"; font.pixelSize: 11; font.bold: true; font.family: "Consolas" }
            Text { text: "X " + fmtCoord(md.posX) + "  Y " + fmtCoord(md.posY); color: "#EEF3F6"; font.pixelSize: 12; font.family: "Consolas" }
            Text { text: "YAW " + fmtCoord(md.posYaw) + "°"; color: "#EEF3F6"; font.pixelSize: 12; font.family: "Consolas" }
            Text { text: "己方伤害 " + md.allyDamage + "  敌方 " + md.enemyDamage; color: "#9AA4AD"; font.pixelSize: 11 }
        }
    }

    // ==================== 事件提示 (顶部中央下方) ====================
    Rectangle {
        id: eventPopup
        anchors.top: centerPanel.bottom
        anchors.topMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: 300
        height: 34
        color: "#1A1A1A"
        opacity: 0
        radius: 5
        border.color: "#FF4444"
        border.width: 1
        visible: md.lastEventId > 0

        Text {
            anchors.centerIn: parent
            text: md.lastEventText
            color: "#FFB3B3"
            font.pixelSize: 12
            font.bold: true
        }

        // 事件到来时短暂弹出后淡出
        Connections {
            target: md
            function onEventChanged() {
                eventPopup.opacity = 0.95
                eventFadeTimer.restart()
            }
        }
        Timer {
            id: eventFadeTimer
            interval: 3000
            onTriggered: eventPopup.opacity = 0
        }
    }
}
