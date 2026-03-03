//
// Created by clemens on 21.01.19.
//

#ifndef SELF_O_MAT_BOOTHGUI_H
#define SELF_O_MAT_BOOTHGUI_H


#include <cstdint>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <queue>
#include "FPSCounter.h"
#include "IGui.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <codecvt>
#include <tools/readfile.h>
#include <logic/ILogicController.h>
#include <tools/verbose.h>

#include "spdlog/sinks/ringbuffer_sink.h"

#ifdef HAVE_QRENCODE
#include <qrencode.h>
#endif

#define DEBUG_QUEUE_SIZE 20

// Modern color palette
#define COLOR_BG            sf::Color(18, 18, 24, 255)
#define COLOR_SURFACE       sf::Color(30, 30, 42, 255)
#define COLOR_PRIMARY       sf::Color(100, 210, 180, 255)
#define COLOR_PRIMARY_DARK  sf::Color(60, 160, 130, 255)
#define COLOR_DANGER        sf::Color(230, 75, 75, 255)
#define COLOR_TEXT          sf::Color(240, 240, 245, 255)
#define COLOR_TEXT_DIM      sf::Color(150, 150, 165, 255)
#define COLOR_MAIN          sf::Color(100, 210, 180, 255)
#define COLOR_MAIN_LIGHT    sf::Color(155, 210, 195, 200)
#define COLOR_ALERT         sf::Color(230, 75, 75, 255)

namespace selfomat {
    namespace ui {

        struct Alert {
            sf::Int32 startTime;
            sf::Int32 endTime;
            std::wstring text;
            bool hint;
        };

        class BoothGui : public IGui {
        public:
            explicit BoothGui(bool fullscreen, bool debug, logic::ILogicController *logicController);

        private:

            static std::string TAG;

            logic::ILogicController *logicController;

            bool isRunning;

            bool debug;
            bool fullscreen;

            bool printerEnabled;
            bool templateEnabled;

            bool shouldShowAgreement;

            // Share / QR info
            bool shouldShowQR = false;
            std::string shareUrl;
            std::string wifiSSID;
            std::string wifiPassword;
            sf::Texture qrWifiTexture;
            sf::Texture qrUrlTexture;
            bool qrWifiReady  = false;
            bool qrUrlReady   = false;

            GUI_STATE currentState;
            sf::Clock stateTimer;

            boost::mutex alertMutex;
            std::map<ALERT_TYPE, Alert> alerts;
            sf::Clock alertTimer;

            sf::VideoMode videoMode;
            sf::RenderWindow window;
            sf::Font hackFont;
            sf::Font iconFont;
            sf::Font mainFont;
            sf::Color clearColor;
            sf::Text debugText;
            sf::Text iconText;
            sf::Text alertText;
            sf::Text printText;
            sf::Text agreementText;
            sf::Texture imageTexture;
            sf::Sprite imageSprite;
            sf::Sprite finalImageSprite;
            sf::Mutex imageMutex;
            sf::RectangleShape rect_overlay;

            sf::CircleShape count_down_circle;

            std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> logging_sink{nullptr};

            // Live frame
            sf::Texture textureLiveOverlay;
            sf::Texture texturePrintOverlay;
            sf::Texture textureFinalImageOverlay;
            sf::Sprite imageSpriteLiveOverlay;
            sf::Sprite imageSpritePrintOverlay;

            bool templateLoaded = false;
            sf::Sprite imageSpriteFinalOverlay;

            sf::Texture textureNoCamera;
            sf::Sprite imageNoCamera;

            std::wstring agreement;

            int finalOverlayOffsetX, finalOverlayOffsetY, finalOverlayOffsetW, finalOverlayOffsetH;


            void *imageBuffer = nullptr;
            uint32_t imageWidth = 0;
            uint32_t imageHeight = 0;
            bool imageDirty = true;
            bool imageShown = false;

            boost::thread renderThreadHandle;

            FPSCounter renderFrameCounter;
            FPSCounter cameraFrameCounter;

            boost::mutex guiStateMutex;

            void renderThread();

            void setState(GUI_STATE newState);

            float easeOutSin(float t, float b, float c, float d);

            void drawShareButtons(float percentage = 1.0f);
            void drawLivePreviewBar();
            void drawShareQR(float alpha = 1.0f);
            void drawAlerts();
            void drawAgreement(float alpha = 1);
            void drawDebug();
            void drawRoundedRect(sf::RenderTarget &target, float x, float y, float w, float h,
                                 float radius, sf::Color fill);

            bool isFertigButton(int mouseX, int mouseY);
            bool isNochmalButton(int mouseX, int mouseY);
            bool isInButtonBar(int mouseY);

            void generateQRTextures();
#ifdef HAVE_QRENCODE
            static sf::Texture buildQRTexture(const std::string &text, int modulePixels = 8);
#endif

            void removeAlert(ALERT_TYPE type, bool forced);

        public:
            BoothGui();


            const GUI_STATE getCurrentGuiState() override {
                boost::unique_lock<boost::mutex> lk(guiStateMutex);
                return currentState;
            }

            void setLogicController(logic::ILogicController *logicController) {
                this->logicController = logicController;
            }


        public:

            void setDebugOutput(bool debug);

            bool start() override;

            void stop() override;

            void initialized() override;

            void reloadTemplate() override;

            void updatePreviewImage(void *data, uint32_t width, uint32_t height) override;

            void hidePreviewImage() override {
                imageMutex.lock();
                imageShown = false;
                setState(STATE_BLACK);
                imageMutex.unlock();
            }

            void notifyFinalImageSent() override {
                setState(STATE_TRANS_BLACK_FINAL);
            }

            void notifyPreviewIncoming() override {
                if (shouldShowQR) {
                    shouldShowQR = false;
                    setState(STATE_SHARE_QR);
                } else if(currentState == STATE_BLACK) {
                    setState(STATE_TRANS_PREV2_PREV3);
                } else {
                    setState(STATE_TRANS_PRINT_PREV1);
                }
            }

            void showAgreement() override;
            void hideAgreement() override;

            bool hasAlert(ALERT_TYPE type) override;
            void addAlert(ALERT_TYPE type, std::wstring text, bool autoRemove = false, bool isHint = false) override;
            void removeAlert(ALERT_TYPE type) override;

            void setPrinterEnabled(bool printerEnabled) override;
            void setTemplateEnabled(bool templateEnabled) override;

            void cancelPrint() override;
            void confirmPrint() override;

            void setShareInfo(const std::string &ssid, const std::string &password, const std::string &url) override {
                wifiSSID    = ssid;
                wifiPassword = password;
                shareUrl    = url;
                // Invalidate cached QR textures so they are regenerated
                qrWifiReady = false;
                qrUrlReady  = false;
            }

            ~BoothGui();


        };
    }
}

#endif //SELF_O_MAT_BOOTHGUI_H
