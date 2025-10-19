// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import "./components" as MoneroComponents
import "./components/effects/" as MoneroEffects
import "./pages"
import "./pages/merchant"
import "./pages/settings"
import QtGraphicalEffects
import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import moneroComponents.Wallet 1.0

Rectangle {
    id: root

    property Item currentView
    property Item previousView
    property int minHeight: (appWindow.height > 800) ? appWindow.height : 800
    property alias contentHeight: mainFlickable.contentHeight
    property alias flickable: mainFlickable
    property Transfer transferView

    transferView: Transfer {
        onPaymentClicked: root.paymentClicked(recipients, paymentId, mixinCount, priority, description)
        onSweepUnmixableClicked: root.sweepUnmixableClicked()
    }

    property Receive receiveView

    receiveView: Receive {
    }

    property Merchant merchantView

    merchantView: Merchant {
    }

    property History historyView

    historyView: History {
    }

    property Advanced advancedView

    advancedView: Advanced {
    }

    property Settings settingsView

    settingsView: Settings {
    }

    property AddressBook addressBookView

    addressBookView: AddressBook {
    }

    property Keys keysView

    keysView: Keys {
    }

    property Account accountView

    accountView: Account {
    }

    signal paymentClicked(var recipients, string paymentId, int mixinCount, int priority, string description)
    signal sweepUnmixableClicked()
    signal generatePaymentIdInvoked()
    signal getProofClicked(string txid, string address, string message, string amount)
    signal checkProofClicked(string txid, string address, string message, string signature)

    function updateStatus() {
        transferView.updateStatus();
    }

    // send from AddressBook
    function sendTo(address, paymentId, description) {
        root.state = "Transfer";
        transferView.sendTo(address, paymentId, description);
    }

    // open Transactions page with search term in search field
    function searchInHistory(searchTerm) {
        root.state = "History";
        historyView.searchInHistory(searchTerm);
    }

    onCurrentViewChanged: {
        if (previousView) {
            if (typeof previousView.onPageClosed === "function")
                previousView.onPageClosed();

        }
        previousView = currentView;
        if (currentView) {
            stackView.replace(currentView);
            // Component.onCompleted is called before wallet is initilized
            if (typeof currentView.onPageCompleted === "function")
                currentView.onPageCompleted();

        }
    }
    states: [
        State {
            name: "History"

            PropertyChanges {
                target: root
                currentView: historyView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: historyView.contentHeight + 80
            }

        },
        State {
            name: "Transfer"

            PropertyChanges {
                target: root
                currentView: transferView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: transferView.transferHeight1 + transferView.transferHeight2 + 80
            }

        },
        State {
            name: "Receive"

            PropertyChanges {
                target: root
                currentView: receiveView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: receiveView.receiveHeight + 80
            }

        },
        State {
            name: "Merchant"

            PropertyChanges {
                target: root
                currentView: merchantView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: merchantView.merchantHeight + 80
            }

        },
        State {
            name: "AddressBook"

            PropertyChanges {
                target: root
                currentView: addressBookView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: addressBookView.addressbookHeight + 80
            }

        },
        State {
            name: "Advanced"

            PropertyChanges {
                target: root
                currentView: advancedView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: advancedView.panelHeight
            }

        },
        State {
            name: "Settings"

            PropertyChanges {
                target: root
                currentView: settingsView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: settingsView.settingsHeight
            }

        },
        State {
            name: "Keys"

            PropertyChanges {
                target: root
                currentView: keysView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: keysView.keysHeight + 80
            }

        },
        State {
            name: "Account"

            PropertyChanges {
                target: root
                currentView: accountView
            }

            PropertyChanges {
                target: mainFlickable
                contentHeight: accountView.accountHeight + 80
            }

        }
    ]
    Rectangle {
        // grey background on merchantView
        visible: currentView === merchantView
        color: MoneroComponents.Style.moneroGrey
        anchors.fill: parent
    }

    MoneroEffects.GradientBackground {
        visible: currentView !== merchantView
        anchors.fill: parent
        fallBackColor: MoneroComponents.Style.middlePanelBackgroundColor
        initialStartColor: MoneroComponents.Style.middlePanelBackgroundGradientStart
        initialStopColor: MoneroComponents.Style.middlePanelBackgroundGradientStop
        blackColorStart: MoneroComponents.Style._b_middlePanelBackgroundGradientStart
        blackColorStop: MoneroComponents.Style._b_middlePanelBackgroundGradientStop
        whiteColorStart: MoneroComponents.Style._w_middlePanelBackgroundGradientStart
        whiteColorStop: MoneroComponents.Style._w_middlePanelBackgroundGradientStop
        start: Qt.point(0, 0)
        end: Qt.point(height, width)
    }

    ColumnLayout {
        // flickable

        anchors.fill: parent
        anchors.margins: {
            if (currentView === merchantView || currentView === historyView)
                return 0;

            return 20;
        }
        anchors.topMargin: appWindow.persistentSettings.customDecorations ? 50 : 0
        anchors.bottomMargin: 0
        spacing: 0

        Flickable {
            id: mainFlickable

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: isMac ? Flickable.DragAndOvershootBounds : Flickable.StopAtBounds
            onFlickingChanged: {
                releaseFocus();
            }

            // Views container
            StackView {
                id: stackView

                initialItem: transferView
                anchors.fill: parent
                clip: true // otherwise animation will affect left panel

                delegate: StackViewDelegate {

                    pushTransition: StackViewTransition {
                        PropertyAnimation {
                            target: enterItem
                            property: "x"
                            from: 0 - target.width
                            to: 0
                            duration: 300
                            easing.type: Easing.OutCubic
                        }

                        PropertyAnimation {
                            target: exitItem
                            property: "x"
                            from: 0
                            to: target.width
                            duration: 300
                            easing.type: Easing.OutCubic
                        }

                    }

                }

            }

            ScrollBar.vertical: ScrollBar {
                parent: root
                anchors.left: parent.right
                anchors.leftMargin: -14 // 10 margin + 4 scrollbar width
                anchors.top: parent.top
                anchors.topMargin: persistentSettings.customDecorations ? 60 : 10
                anchors.bottom: parent.bottom
                anchors.bottomMargin: persistentSettings.customDecorations ? 15 : 10
                onActiveChanged: {
                    if (!active && !isMac) {
                        active = true;
                    }
                }
            }

        }

    }

    // border
    Rectangle {
        id: borderLeft

        visible: middlePanel.state !== "Merchant"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 1
        color: MoneroComponents.Style.appWindowBorderColor

        MoneroEffects.ColorTransition {
            targetObj: parent
            blackColor: MoneroComponents.Style._b_appWindowBorderColor
            whiteColor: MoneroComponents.Style._w_appWindowBorderColor
        }

    }

    // border shadow
    Image {
        source: "qrc:///images/middlePanelShadow.png"
        width: 12
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: borderLeft.right
    }

}
