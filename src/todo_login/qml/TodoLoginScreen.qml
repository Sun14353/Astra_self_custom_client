// ============================================================
// todo_login / TodoLoginScreen.qml
// 架构骨架：不同选手的登录选择界面。
//
// TODO:
//   - 选手列表改为从 playerModel.availablePlayers() 动态加载 todo_presets
//   - 确认后回传选定预设，联动 todo_control 加载对应控制映射
// ============================================================
import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    color: "#0f1419"

    // TODO: 接入 playerModel.availablePlayers() 作为动态数据源；
    // 当前使用占位 ListModel 展示界面结构。
    ListModel {
        id: placeholderPlayers
        ListElement { name: "默认操作手"; desc: "均衡型"; preset: "todo_default" }
        ListElement { name: "狙击手"; desc: "低灵敏度"; preset: "todo_sniper" }
        ListElement { name: "进攻手"; desc: "高灵敏度"; preset: "todo_aggressive" }
        ListElement { name: "支援手"; desc: "折中"; preset: "todo_support" }
    }

    Column {
        anchors.centerIn: parent
        spacing: 24

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "选择操作手"
            color: "#e6edf3"
            font.pixelSize: 28
            font.bold: true
        }

        // 选手/预设列表（骨架）
        ListView {
            id: playerList
            width: 360
            height: 240
            clip: true
            spacing: 4
            // TODO: model: playerModel.availablePlayers()
            model: placeholderPlayers
            delegate: ItemDelegate {
                width: playerList.width
                height: 52
                text: model.name + "   —   " + model.desc
                onClicked: {
                    // TODO: 回传选定预设 id，联动 todo_control
                    console.log("selected preset:", model.preset)
                }
            }
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "进入"
            onClicked: {
                // TODO: 确认登录，加载选定选手预设
            }
        }
    }
}
