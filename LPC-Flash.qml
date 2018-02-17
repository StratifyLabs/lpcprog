import QtQuick 2.0
import StratifyLabs.UI 2.1
import QtCharts 2.2
import QtQuick.Dialogs 1.2


SContainer {
  name: "Test";
  style: "block fill padding-zero";

  function startProgramming(){
    var args = "-uart " + uartPort.text +
        " -message 1.0" +
        " -r " + resetPin.text +
        " -i " + ispreqPin.text +
        (rxPin.text ? " -rx " + rxPin.text : "") +
        (txPin.text ? " -tx " + txPin.text : "") +
        (bitrate.text ? " -f " + bitrate.text : "") +
        (device.text ? " -d " + device.text : "") +
        " -in " + "/home/" + imageFile.text.split('/').pop();

    logic.appRun("lpcprog " + args);
    logic.setState("lpcprog.binaryImage", imageFile.text);
    logic.setState("lpcprog.uartPort", uartPort.text);
    logic.setState("lpcprog.resetPin", resetPin.text);
    logic.setState("lpcprog.ispreqPin", ispreqPin.text);
    logic.setState("lpcprog.bitrate", bitrate.text);
    logic.setState("lpcprog.rxPin", rxPin.text);
    logic.setState("lpcprog.txPin", txPin.text);
    logic.setState("lpcprog.device", device.text);
    console.log("Run lpcprog " + args);
  }

  function reset(){
    var args = "-reset " +
        " -message 1.0" +
        " -r " + resetPin.text;

    logic.appRun("lpcprog " + args);
  }


  Connections {
    target: logic;
    onCopyFileToDeviceCompleted: {
      console.log("File Copy Complete: Run App");

      if( logic.isAppRunning("uartprobe") ){
        logic.appKill("uartprobe");
      } else {
        startProgramming();
      }
    }

    onAppStopped: {
      if( name === "lpcprog" ){
        status.text = "";
        logic.appRun("uartprobe -uart " + uartPort.text);
      }

      if( name === "uartprobe" ){
        startProgramming();
      }
    }

    onDeviceMessageChanged: {
      console.log("Got a message " + message.type);
      if( message.type === "progress" ){
        logic.progress = message.progress;
        logic.progressMax = message.max;
        logic.progressChanged();
      } else if ( message.type === "status" ){
        logic.progress = 0;
        logic.progressMax = 0;
        logic.progressChanged();
        console.log("Status: " + message.status);
        status.text = message.status;
      }
    }

    onAppInstalled: {
      console.log("Installed: " + name);
      if( name == "lpcprog" ){
        logic.appInstall("/Users/tgil/git/StratifyApps/uartprobe");
      }
    }

  }

  Component.onCompleted: {
    imageFile.text = logic.getState("lpcprog.binaryImage");
    uartPort.text = logic.getState("lpcprog.uartPort");
    resetPin.text = logic.getState("lpcprog.resetPin");
    ispreqPin.text = logic.getState("lpcprog.ispreqPin");
    bitrate.text = logic.getState("lpcprog.bitrate");
    rxPin.text = logic.getState("lpcprog.rxPin");
    txPin.text = logic.getState("lpcprog.txPin");
    device.text = logic.getState("lpcprog.device");
  }


  SColumn {
    style: "block fill";
    SContainer {
      style: "block fill";
      SColumn {
        style: "block fill";
        SRow {
          SIcon {
            attr.paddingHorizontal: 0;
            style: "text-h1 left text-bold";
            icon: Fa.Icon.microchip;
            label: " LPC Flash Programmer"
          }
        }

        SHLine{}

        SRow {
          SLabel {
            style: "btn-primary left";
            text: "Options";
            attr.paddingHorizontal: 0;
            span: 2;
          }

          SGroup {
            span: 2;
            style: "right";

            SButton {
              id: showButton;
              icon: options.visible ? Fa.Icon.minus: Fa.Icon.plus;
              style: "btn-outline-secondary text-semi-bold";
              onClicked: {
                options.visible = !options.visible;
              }
            }
          }
        }

        SContainer {
          id: options;
          attr.paddingHorizontal: 0;
          SColumn {
            SRow {
              SLabel {
                span: 2;
                attr.paddingHorizontal: 0;
                text: "Firmware Update";
                style: "left";
              }

              SGroup {
                span: 2;
                style: "right";
                SButton {
                  span: 2;
                  label: "Browse";
                  icon: Fa.Icon.folder_open;
                  style: "btn-outline-secondary right text-semi-bold";
                  onClicked: imageDialog.visible = true;

                  FileDialog {
                    id: imageDialog;
                    onAccepted: {
                      imageFile.text = fileUrl;
                    }
                  }
                }
              }
            }

            SInput {
              id: imageFile;
              style: "btn-outline-secondary right";
              placeholder: "Binary Image File";
            }
            SRow {
              SLabel {
                span: 2;
                style: "left";
                text: "UART";
              }
              SInput {
                id: uartPort;
                span:2;
                style: "right text-center";
              }
              SLabel {
                span: 2;
                style: "left";
                text: "Bitrate";
              }
              SInput {
                id: bitrate;
                span:2;
                style: "right text-center";
              }
              SLabel {
                span: 2;
                style: "left";
                text: "RX";
              }
              SInput {
                id: rxPin;
                span:2;
                style: "right text-center";
              }
              SLabel {
                span: 2;
                style: "left";
                text: "TX";
              }
              SInput {
                id: txPin;
                span:2;
                style: "right text-center";
              }
              SLabel {
                span: 2;
                style: "left";
                text: "Reset";
              }
              SInput {
                id: resetPin;
                span:2;
                style: "right text-center";
              }
              SLabel {
                span: 2;
                style: "left";
                text: "ISP Request";
              }
              SInput {
                id: ispreqPin;
                span:2;
                style: "right text-center";
              }
              SLabel {
                span: 2;
                style: "left";
                text: "Device";
              }
              SInput {
                id: device;
                span:2;
                style: "right text-center";
              }
            }
          }
        }

        SHLine{}

        SRow {
          SLabel {
            span: 2;
            style: "left";
            text: "UART Terminal";
            attr.paddingHorizontal: 0;
          }

          SGroup {
            span: 2;
            style: "right";
            SButton {
              style: "btn-outline-secondary right text-semi-bold";
              text: "Clear";
              icon: Fa.Icon.times;
              onClicked: {
                terminalTextBox.textBox.clear();
              }
            }

            SButton {
              style: "btn-outline-secondary right text-semi-bold";
              text: "Run";
              icon: Fa.Icon.play;
              onClicked: {
                logic.appRun("uartprobe -uart " + uartPort.text);
              }
            }
          }
        }

        SInput {
          id: executeInput;
          placeholder: "Send";
          Keys.onReturnPressed: {
            //write to the UART
            logic.stdioWrite(text + "\n");
            text = "";
          }
        }

        STextBox {
          id: terminalTextBox;
          style: "fill";
          implicitHeight: 20;
          attr.textFont: STheme.font_family_monospace.name;
          textBox.readOnly: true;
          Connections {
            target: logic;
            onStdioChanged: {
              terminalTextBox.textBox.insert(terminalTextBox.textBox.length, value);
            }
          }
        }

        SLabel {
          attr.fontSize: 30;
          style: "label-lg";
          text: " ";
        }

      }
    }
  }

  SContainer {
    id: bottomBar;
    color: STheme.gray_lighter;
    anchors.bottom: parent.bottom;

    SColumn {
      SRow {
        SText {
          id: status;
          visible: text !== "";
          style: "text-semi-bold";
        }

        SProgressBar {
          span: 12;
          visible: logic.progressMax != 0;
          value: logic.progressMax ? logic.progress / logic.progressMax : 0;
          style: "block success sm";
        }
      }

      SRow {
        style: "bottom";
        SGroup {
          style: "left";
          span: 2;
          SButton {
            style: "btn-primary";
            label: "Install";
            onClicked: {
              logic.appInstall("/Users/tgil/git/StratifyApps/lpcprog");
            }
          }
        }

        SGroup {
          style: "right";
          span: 2;

          SButton {
            style: "btn-naked";
            icon: Fa.Icon.times;
            label: "Abort";
            onClicked: {
              var message = {
                "command": "abort",
                "text": "value"
              };
              logic.sendMessageToDevice(1, message);
            }
          }

          SButton {
            style: "btn-danger";
            icon: Fa.Icon.repeat;
            label: "Reset";
            onClicked: reset();
          }

          SButton {
            style: "btn-success text-semi-bold";
            icon: Fa.Icon.download;
            label: "Program!!";
            onClicked: {
              //download the image
              var dest = "/home/" + imageFile.text.split('/').pop();
              logic.copyToDevice(imageFile.text, dest);
            }
          }
        }
      }
    }

  }
}
