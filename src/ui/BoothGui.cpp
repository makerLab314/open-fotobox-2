//
// Created by clemens on 21.01.19.
//

#include "BoothGui.h"
#include "tools/verbose.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/dist_sink.h>
#include <cmath>

using namespace std;
using namespace selfomat::ui;
using namespace selfomat::tools;

std::string BoothGui::TAG = "BOOTH_GUI";

// --- Helper: draw rounded rectangle ----------------------------------------
void BoothGui::drawRoundedRect(sf::RenderTarget &target, float x, float y,
                               float w, float h, float r, sf::Color fill)
{
    sf::RectangleShape hBar(sf::Vector2f(w - 2*r, h));
    hBar.setPosition(x + r, y);
    hBar.setFillColor(fill);
    target.draw(hBar);

    sf::RectangleShape vBar(sf::Vector2f(w, h - 2*r));
    vBar.setPosition(x, y + r);
    vBar.setFillColor(fill);
    target.draw(vBar);

    struct Corner { float cx, cy; };
    Corner corners[4] = {
        {x + r,     y + r    },
        {x + w - r, y + r    },
        {x + r,     y + h - r},
        {x + w - r, y + h - r}
    };
    for (auto &c : corners) {
        sf::CircleShape circle(r);
        circle.setFillColor(fill);
        circle.setPosition(c.cx - r, c.cy - r);
        target.draw(circle);
    }
}

// --- QR code texture generation --------------------------------------------
#ifdef HAVE_QRENCODE
sf::Texture BoothGui::buildQRTexture(const std::string &text, int px)
{
    QRcode *qr = QRcode_encodeString(text.c_str(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    sf::Texture tex;
    if (!qr) return tex;

    int sz   = qr->width;
    int pad  = 4;
    int total = (sz + 2 * pad) * px;

    sf::Image img;
    img.create(total, total, sf::Color::White);

    for (int row = 0; row < sz; row++) {
        for (int col = 0; col < sz; col++) {
            if (qr->data[row * sz + col] & 1) {
                for (int dy = 0; dy < px; dy++) {
                    for (int dx = 0; dx < px; dx++) {
                        img.setPixel((col + pad) * px + dx,
                                     (row + pad) * px + dy,
                                     sf::Color::Black);
                    }
                }
            }
        }
    }
    QRcode_free(qr);
    tex.loadFromImage(img);
    return tex;
}
#endif

void BoothGui::generateQRTextures()
{
#ifdef HAVE_QRENCODE
    if (!wifiSSID.empty()) {
        std::string wifiData = "WIFI:T:WPA;S:" + wifiSSID + ";P:" + wifiPassword + ";;";
        qrWifiTexture = buildQRTexture(wifiData, 8);
        qrWifiReady   = qrWifiTexture.getSize().x > 0;
    }
    if (!shareUrl.empty()) {
        qrUrlTexture = buildQRTexture(shareUrl, 8);
        qrUrlReady   = qrUrlTexture.getSize().x > 0;
    }
#endif
}

// --- Button hit-test helpers -----------------------------------------------
bool BoothGui::isFertigButton(int mx, int my)
{
    float winH = (float)window.getSize().y;
    float btnW = 260.f, btnH = 68.f;
    float barH = 120.f;
    float btnX = window.getSize().x / 2.0f + 20.f;
    float btnY = winH - barH + (barH - btnH) / 2.f;
    return mx >= btnX && mx <= btnX + btnW &&
           my >= btnY && my <= btnY + btnH;
}

bool BoothGui::isNochmalButton(int mx, int my)
{
    float winH = (float)window.getSize().y;
    float btnW = 240.f, btnH = 68.f;
    float barH = 120.f;
    float btnX = window.getSize().x / 2.0f - btnW - 20.f;
    float btnY = winH - barH + (barH - btnH) / 2.f;
    return mx >= btnX && mx <= btnX + btnW &&
           my >= btnY && my <= btnY + btnH;
}

// --- Constructor / Destructor ----------------------------------------------
BoothGui::BoothGui(bool fullscreen, bool debug, logic::ILogicController *logicController) {
    videoMode = sf::VideoMode(1280, 800);
    this->currentState = STATE_INIT;
    this->shouldShowAgreement = false;
    this->debug = debug;
    this->logicController = logicController;
    this->fullscreen = fullscreen;

    logging_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(DEBUG_QUEUE_SIZE);
    logging_sink->set_pattern("[%X] [%^%l%$]: %v");
    dynamic_cast<spdlog::sinks::dist_sink<std::mutex>*>(
        spdlog::default_logger()->sinks().front().get())->add_sink(logging_sink);
}

BoothGui::~BoothGui() {
    dynamic_cast<spdlog::sinks::dist_sink<std::mutex>*>(
        spdlog::default_logger()->sinks().front().get())->remove_sink(logging_sink);
}

// --- start -----------------------------------------------------------------
bool BoothGui::start() {
    if (!hackFont.loadFromFile("./assets/Hack-Regular.ttf"))      { LOG_E(TAG, "Font load error"); return false; }
    if (!iconFont.loadFromFile("./assets/self-o-mat.ttf"))        { LOG_E(TAG, "Font load error"); return false; }
    if (!mainFont.loadFromFile("./assets/AlegreyaSans-Bold.ttf")) { LOG_E(TAG, "Font load error"); return false; }

    if (!textureLiveOverlay.loadFromFile("./assets/live.png"))         { LOG_E(TAG, "live asset error"); return false; }
    imageSpriteLiveOverlay.setTexture(textureLiveOverlay);

    if (!texturePrintOverlay.loadFromFile("./assets/print_overlay.png")) { LOG_E(TAG, "print overlay error"); return false; }
    imageSpritePrintOverlay.setTexture(texturePrintOverlay);

    if (!textureNoCamera.loadFromFile("./assets/no-camera.png")) { LOG_E(TAG, "no-camera asset error"); return false; }
    imageNoCamera.setTexture(textureNoCamera);

    readFile("./assets/agreement.txt", agreement);
    if (agreement.length() < 1) { LOG_E(TAG, "agreement text error"); return false; }

    reloadTemplate();

    rect_overlay      = sf::RectangleShape(sf::Vector2f(videoMode.width, videoMode.height));
    count_down_circle = sf::CircleShape(19.0f);

    isRunning = true;
    renderThreadHandle = boost::thread(boost::bind(&BoothGui::renderThread, this));
    return true;
}

// --- stop ------------------------------------------------------------------
void BoothGui::stop() {
    isRunning = false;
    if (renderThreadHandle.joinable()) {
        LOG_I(TAG, "Waiting for gui to stop");
        renderThreadHandle.join();
    }
}

// --- reloadTemplate --------------------------------------------------------
void BoothGui::reloadTemplate() {
    imageMutex.lock();
    templateLoaded = textureFinalImageOverlay.loadFromFile(
        std::string(getenv("HOME")) + "/.template_screen.png");
    if (!templateLoaded) {
        LOG_E(TAG, "Could not load screen template asset.");
    } else {
        imageSpriteFinalOverlay.setTexture(textureFinalImageOverlay);
    }
    if (templateLoaded) {
        boost::property_tree::ptree ptree;
        try {
            boost::property_tree::read_json(
                std::string(getenv("HOME")) + "/.template_screen_props.json", ptree);
            finalOverlayOffsetX = ptree.get<int>("offset_x");
            finalOverlayOffsetY = ptree.get<int>("offset_y");
            finalOverlayOffsetW = ptree.get<int>("offset_w");
            finalOverlayOffsetH = ptree.get<int>("offset_h");
        } catch (boost::exception &e) {
            LOG_E(TAG, std::string("Error loading template properties: ") +
                           boost::diagnostic_information(e));
        }
    }
    imageMutex.unlock();
}

// --- updatePreviewImage ----------------------------------------------------
void BoothGui::updatePreviewImage(void *data, uint32_t width, uint32_t height) {
    if (getCurrentGuiState() == STATE_TRANS_PRINT_PREV1) return;
    imageMutex.lock();
    imageBuffer = data;
    imageWidth  = width;
    imageHeight = height;
    imageDirty  = true;
    imageShown  = true;
    imageMutex.unlock();

    if (getCurrentGuiState() == STATE_TRANS_PREV1_PREV2)
        setState(STATE_TRANS_PREV2_PREV3);
}

// --- renderThread ----------------------------------------------------------
void BoothGui::renderThread() {
    LOG_I(TAG, "Render thread started!");
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    settings.majorVersion = 2;
    settings.minorVersion = 1;
    auto style = fullscreen ? sf::Style::Fullscreen : sf::Style::Default;

    window.create(videoMode, "FotoBox", style, settings);
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(60);

    clearColor = COLOR_BG;

    debugText.setFont(hackFont);
    debugText.setFillColor(sf::Color::White);
    debugText.setOutlineColor(sf::Color::Black);
    debugText.setOutlineThickness(1.5f);
    debugText.setCharacterSize(15);

    iconText.setFont(iconFont);
    iconText.setFillColor(COLOR_ALERT);
    iconText.setCharacterSize(50);

    alertText.setFont(mainFont);
    alertText.setFillColor(COLOR_ALERT);
    alertText.setCharacterSize(46);
    alertText.setStyle(sf::Text::Bold);

    printText.setFont(mainFont);
    printText.setFillColor(COLOR_PRIMARY);
    printText.setCharacterSize(72);
    printText.setStyle(sf::Text::Bold);

    agreementText.setFont(hackFont);
    agreementText.setFillColor(sf::Color::White);
    agreementText.setStyle(sf::Text::Bold);

    imageTexture.create(videoMode.width, videoMode.height);
    imageSprite = sf::Sprite(imageTexture);

    generateQRTextures();

    window.setActive(true);
    sf::Event event{};
    stateTimer.restart();

    while (isRunning) {

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (logicController != nullptr &&
                       event.type == sf::Event::MouseButtonPressed &&
                       event.mouseButton.button == sf::Mouse::Button::Left) {

                int mx = event.mouseButton.x;
                int my = event.mouseButton.y;
                auto state = getCurrentGuiState();

                if (logicController->isAgreementVisible()) {
                    logicController->acceptAgreement();

                } else if (state == STATE_SHARE_QR) {
                    // Tap anywhere on QR screen -> back to live preview
                    shouldShowQR = false;
                    setState(STATE_TRANS_PREV2_PREV3);

                } else if (state == STATE_FINAL_IMAGE_PRINT ||
                           state == STATE_FINAL_IMAGE_PRINT_CANCELED ||
                           state == STATE_FINAL_IMAGE_PRINT_CONFIRMED ||
                           state == STATE_FINAL_IMAGE) {
                    if (isFertigButton(mx, my)) {
                        shouldShowQR = true;
                        logicController->cancelPrint();
                    } else if (isNochmalButton(mx, my)) {
                        logicController->cancelPrint();
                    }
                    // else: touch elsewhere during photo review -> ignore

                } else if (state == STATE_LIVE_PREVIEW) {
                    if (isFertigButton(mx, my)) {
                        setState(STATE_SHARE_QR);
                    } else {
                        logicController->trigger();
                    }
                }
            }
        }

        window.clear(clearColor);

        renderFrameCounter.nextFrame();
        if (imageDirty) {
            cameraFrameCounter.nextFrame();
            imageMutex.lock();
            if (imageWidth > 0 && imageHeight > 0) {
                auto tsz = imageTexture.getSize();
                if (tsz.x != imageWidth || tsz.y != imageHeight) {
                    if (imageWidth > tsz.x || imageHeight > tsz.y) {
                        imageTexture.create(imageWidth, imageHeight);
                    }
                }

                float scaleX = (float)window.getSize().x / (float)imageWidth;
                float scaleY = (float)window.getSize().y / (float)imageHeight;
                float scale  = std::max(scaleX, scaleY);

                imageSprite.setScale(-scale, scale);

                float wCX = window.getSize().x / 2.0f;
                float wCY = window.getSize().y / 2.0f;
                float iCX = (float)imageWidth  * scale * 0.5f;
                float iCY = (float)imageHeight * scale * 0.5f;

                imageSprite.setPosition(videoMode.width - (wCX - iCX), wCY - iCY);

                if (templateEnabled && templateLoaded) {
                    float foCX = (float)finalOverlayOffsetX + (float)finalOverlayOffsetW / 2.0f;
                    float foCY = (float)finalOverlayOffsetY + (float)finalOverlayOffsetH / 2.0f;

                    finalImageSprite.setTexture(imageTexture, true);
                    finalImageSprite.setTextureRect(sf::IntRect(0, 0, imageWidth, imageHeight));

                    float fScaleX = (float)finalOverlayOffsetW / (float)imageWidth;
                    float fScaleY = (float)finalOverlayOffsetH / (float)imageHeight;
                    float fScale  = std::max(fScaleX, fScaleY);
                    finalImageSprite.setScale(fScale, fScale);

                    float fICX = (float)imageWidth  * fScale * 0.5f;
                    float fICY = (float)imageHeight * fScale * 0.5f;
                    finalImageSprite.setPosition(foCX - fICX, foCY - fICY);
                } else {
                    finalImageSprite.setTexture(imageTexture, true);
                    finalImageSprite.setTextureRect(sf::IntRect(0, 0, imageWidth, imageHeight));
                    finalImageSprite.setScale(scale, scale);
                    finalImageSprite.setPosition(wCX - iCX, wCY - iCY);
                }

                imageTexture.update((sf::Uint8*)imageBuffer, imageWidth, imageHeight, 0, 0);
                imageDirty = false;
            }
            imageMutex.unlock();
        }

        switch (getCurrentGuiState()) {

        case STATE_LIVE_PREVIEW: {
            auto alert = alerts.find(ALERT_CAMERA);
            if (alert != alerts.end()) {
                window.draw(imageNoCamera);
            } else if (imageShown) {
                window.draw(imageSprite);

                float t  = stateTimer.getElapsedTime().asMilliseconds();
                float a  = std::min(1.0f, t / 300.0f);
                float ac = 0.4f * std::cos(t / 800.0f) + 0.6f;
                imageSpriteLiveOverlay.setColor(sf::Color(255, 255, 255, (uint8_t)(a * ac * 255.f)));
                window.draw(imageSpriteLiveOverlay);

                drawShareButtons(1.0f);
            }
        } break;

        case STATE_BLACK:
            break;

        case STATE_TRANS_BLACK_FINAL: {
            float t     = stateTimer.getElapsedTime().asMilliseconds();
            auto  alpha = (uint8_t)((1.0f - std::min(1.0f, t / 300.0f)) * 255.f);

            window.draw(finalImageSprite);
            if (templateEnabled && templateLoaded) window.draw(imageSpriteFinalOverlay);

            rect_overlay.setFillColor(sf::Color(0, 0, 0, alpha));
            window.draw(rect_overlay);

            if (t > 300.f) setState(STATE_FINAL_IMAGE);
        } break;

        case STATE_FINAL_IMAGE: {
            window.draw(finalImageSprite);
            if (templateEnabled && templateLoaded) window.draw(imageSpriteFinalOverlay);

            drawShareButtons(1.0f);

            float t = stateTimer.getElapsedTime().asMilliseconds();
            if (t >= 1200.f) setState(STATE_TRANS_FINAL_IMAGE_PRINT);
        } break;

        case STATE_TRANS_FINAL_IMAGE_PRINT: {
            float t   = stateTimer.getElapsedTime().asMilliseconds();
            float dur = 350.f;
            float pct = easeOutSin(std::min(1.0f, t / dur), 0.f, 1.f, 1.f);

            window.draw(finalImageSprite);
            if (templateEnabled && templateLoaded) window.draw(imageSpriteFinalOverlay);

            drawShareButtons(pct);

            if (t >= dur) setState(STATE_FINAL_IMAGE_PRINT);
        } break;

        case STATE_FINAL_IMAGE_PRINT: {
            float t = stateTimer.getElapsedTime().asMilliseconds();

            window.draw(finalImageSprite);
            if (templateEnabled && templateLoaded) window.draw(imageSpriteFinalOverlay);

            drawShareButtons(1.0f);

            if (t >= 2500.f) setState(STATE_FINAL_IMAGE_PRINT_CANCELED);
        } break;

        case STATE_FINAL_IMAGE_PRINT_CONFIRMED:
        case STATE_FINAL_IMAGE_PRINT_CANCELED: {
            float t   = stateTimer.getElapsedTime().asMilliseconds();
            float pct = 1.f - std::min(1.0f, t / 200.f);

            window.draw(finalImageSprite);
            if (templateEnabled && templateLoaded) window.draw(imageSpriteFinalOverlay);

            drawShareButtons(pct);
        } break;

        case STATE_TRANS_PRINT_PREV1: {
            float t   = stateTimer.getElapsedTime().asMilliseconds();
            float dur = 250.f;
            float a   = std::max(0.f, std::min(255.f, t / dur * 255.f));

            window.draw(finalImageSprite);
            if (templateEnabled && templateLoaded) window.draw(imageSpriteFinalOverlay);

            rect_overlay.setFillColor(sf::Color(0, 0, 0, (uint8_t)a));
            window.draw(rect_overlay);

            if (t >= dur && imageShown) {
                imageShown = false;
                setState(STATE_TRANS_PREV1_PREV2);
            }
        } break;

        case STATE_TRANS_PREV1_PREV2:
            break;

        case STATE_TRANS_PREV2_PREV3: {
            float t     = stateTimer.getElapsedTime().asMilliseconds();
            auto  alpha = (uint8_t)((1.0f - std::min(1.0f, t / 300.0f)) * 255.f);

            window.draw(imageSprite);
            rect_overlay.setFillColor(sf::Color(0, 0, 0, alpha));
            window.draw(rect_overlay);

            if (t > 300.f) setState(STATE_LIVE_PREVIEW);
        } break;

        case STATE_SHARE_QR: {
            float t = stateTimer.getElapsedTime().asMilliseconds();
            drawShareQR(std::min(1.0f, t / 400.0f));
        } break;

        case STATE_AGREEMENT: {
            float t = stateTimer.getElapsedTime().asMilliseconds();
            drawAgreement(std::min(1.0f, t / 300.0f));
        } break;

        case STATE_TRANS_AGREEMENT: {
            float t = stateTimer.getElapsedTime().asMilliseconds();
            float a = std::min(1.0f, t / 300.0f);
            drawAgreement(1.f - a);
            if (t > 300.f) setState(STATE_TRANS_PREV2_PREV3);
        } break;

        case STATE_INIT:
            if (debug) window.clear(sf::Color(0, 255, 0));
            break;

        default:
            if (debug) window.clear(sf::Color(255, 0, 0));
            break;
        }

        drawAlerts();
        drawDebug();
        window.display();
    }
    window.close();
}

// --- setState --------------------------------------------------------------
void BoothGui::setState(GUI_STATE newState) {
    boost::unique_lock<boost::mutex> lk(guiStateMutex);
    currentState = newState;
    stateTimer.restart();
}

// --- initialized -----------------------------------------------------------
void BoothGui::initialized() {
    if (shouldShowAgreement) {
        setState(STATE_AGREEMENT);
        shouldShowAgreement = false;
    } else {
        setState(STATE_TRANS_PREV2_PREV3);
    }
}

// --- drawShareButtons (replaces old print overlay) -------------------------
void BoothGui::drawShareButtons(float percentage)
{
    if (percentage <= 0.f) return;
    auto alpha = (uint8_t)(std::min(1.0f, percentage) * 220.f);

    float winW = (float)window.getSize().x;
    float winH = (float)window.getSize().y;
    float barH = 120.f;
    float barY = winH - barH * std::min(1.0f, percentage);

    sf::RectangleShape bar(sf::Vector2f(winW, barH + 2.f));
    bar.setFillColor(sf::Color(12, 12, 20, alpha));
    bar.setPosition(0, barY);
    window.draw(bar);

    float btnH     = 68.f;
    float btnY     = barY + (barH - btnH) / 2.0f;

    // "Nochmal" button (left of center)
    float nochmalW = 240.f;
    float nochmalX = winW / 2.0f - nochmalW - 20.f;
    drawRoundedRect(window, nochmalX, btnY, nochmalW, btnH, 12.f,
                    sf::Color(50, 55, 65, alpha));

    sf::Text nochmalLabel;
    nochmalLabel.setFont(mainFont);
    nochmalLabel.setCharacterSize(32);
    nochmalLabel.setFillColor(sf::Color(200, 200, 210, alpha));
    nochmalLabel.setString(L"Nochmal");
    auto nb = nochmalLabel.getLocalBounds();
    nochmalLabel.setPosition(nochmalX + (nochmalW - nb.width) / 2.f,
                              btnY + (btnH - nb.height) / 2.f - 4.f);
    window.draw(nochmalLabel);

    // "Fertig" button (right of center)
    float fertigW = 260.f;
    float fertigX = winW / 2.0f + 20.f;
    drawRoundedRect(window, fertigX, btnY, fertigW, btnH, 12.f,
                    sf::Color(40, 160, 130, alpha));

    sf::Text fertigLabel;
    fertigLabel.setFont(mainFont);
    fertigLabel.setCharacterSize(32);
    fertigLabel.setFillColor(sf::Color(255, 255, 255, alpha));
    fertigLabel.setString(L"Fertig  >>");
    auto fb = fertigLabel.getLocalBounds();
    fertigLabel.setPosition(fertigX + (fertigW - fb.width) / 2.f,
                             btnY + (btnH - fb.height) / 2.f - 4.f);
    window.draw(fertigLabel);
}

// --- drawShareQR -----------------------------------------------------------
void BoothGui::drawShareQR(float alpha)
{
    auto a8 = (uint8_t)(alpha * 255.f);
    float winW = (float)window.getSize().x;
    float winH = (float)window.getSize().y;

    sf::RectangleShape bg(sf::Vector2f(winW, winH));
    bg.setFillColor(sf::Color(12, 12, 20, a8));
    window.draw(bg);

    // Title
    sf::Text title;
    title.setFont(mainFont);
    title.setCharacterSize(54);
    title.setFillColor(sf::Color(240, 240, 245, a8));
    title.setString(L"Fotos herunterladen");
    auto tbounds = title.getLocalBounds();
    title.setPosition((winW - tbounds.width) / 2.f, 40.f);
    window.draw(title);

    float qrSize = 260.f;
    float cardPad = 24.f;
    float cardW   = qrSize + 2.f * cardPad;
    float cardH   = qrSize + 2.f * cardPad + 80.f;
    float gap     = 80.f;
    float totalW  = 2.f * cardW + gap;
    float startX  = (winW - totalW) / 2.f;
    float cardY   = 130.f;

    // WiFi QR card
    {
        drawRoundedRect(window, startX, cardY, cardW, cardH, 16.f,
                        sf::Color(30, 32, 44, a8));

        sf::Text label;
        label.setFont(mainFont);
        label.setCharacterSize(28);
        label.setFillColor(sf::Color(100, 210, 180, a8));
        label.setString(L"WLAN beitreten");
        auto lb = label.getLocalBounds();
        label.setPosition(startX + (cardW - lb.width) / 2.f, cardY + 12.f);
        window.draw(label);

#ifdef HAVE_QRENCODE
        if (qrWifiReady) {
            float qrRatio = qrSize / (float)qrWifiTexture.getSize().x;
            sf::Sprite qrSprite(qrWifiTexture);
            qrSprite.setScale(qrRatio, qrRatio);
            qrSprite.setPosition(startX + cardPad, cardY + cardPad + 44.f);
            qrSprite.setColor(sf::Color(255, 255, 255, a8));
            window.draw(qrSprite);
        } else {
#endif
            sf::RectangleShape ph(sf::Vector2f(qrSize, qrSize));
            ph.setFillColor(sf::Color(255, 255, 255, a8));
            ph.setPosition(startX + cardPad, cardY + cardPad + 44.f);
            window.draw(ph);
#ifdef HAVE_QRENCODE
        }
#endif

        sf::Text ssidText;
        ssidText.setFont(hackFont);
        ssidText.setCharacterSize(20);
        ssidText.setFillColor(sf::Color(200, 200, 215, a8));
        std::string wifiLine = wifiSSID.empty() ? "(kein WLAN konfiguriert)" : wifiSSID;
        if (!wifiPassword.empty()) wifiLine += "\nPW: " + wifiPassword;
        ssidText.setString(wifiLine);
        auto sb = ssidText.getLocalBounds();
        ssidText.setPosition(startX + (cardW - sb.width) / 2.f,
                             cardY + cardPad + 44.f + qrSize + 8.f);
        window.draw(ssidText);
    }

    // Download URL QR card
    float card2X = startX + cardW + gap;
    {
        drawRoundedRect(window, card2X, cardY, cardW, cardH, 16.f,
                        sf::Color(30, 32, 44, a8));

        sf::Text label;
        label.setFont(mainFont);
        label.setCharacterSize(28);
        label.setFillColor(sf::Color(100, 210, 180, a8));
        label.setString(L"Fotos laden");
        auto lb = label.getLocalBounds();
        label.setPosition(card2X + (cardW - lb.width) / 2.f, cardY + 12.f);
        window.draw(label);

#ifdef HAVE_QRENCODE
        if (qrUrlReady) {
            float qrRatio = qrSize / (float)qrUrlTexture.getSize().x;
            sf::Sprite qrSprite(qrUrlTexture);
            qrSprite.setScale(qrRatio, qrRatio);
            qrSprite.setPosition(card2X + cardPad, cardY + cardPad + 44.f);
            qrSprite.setColor(sf::Color(255, 255, 255, a8));
            window.draw(qrSprite);
        } else {
#endif
            sf::RectangleShape ph(sf::Vector2f(qrSize, qrSize));
            ph.setFillColor(sf::Color(255, 255, 255, a8));
            ph.setPosition(card2X + cardPad, cardY + cardPad + 44.f);
            window.draw(ph);
#ifdef HAVE_QRENCODE
        }
#endif

        sf::Text urlText;
        urlText.setFont(hackFont);
        urlText.setCharacterSize(18);
        urlText.setFillColor(sf::Color(180, 180, 200, a8));
        std::string urlDisplay = shareUrl.empty() ? "(URL nicht verfuegbar)" : shareUrl;
        urlText.setString(urlDisplay);
        auto ub = urlText.getLocalBounds();
        if (ub.width > cardW - 2.f * cardPad) {
            urlText.setCharacterSize(14);
            ub = urlText.getLocalBounds();
        }
        urlText.setPosition(card2X + (cardW - ub.width) / 2.f,
                            cardY + cardPad + 44.f + qrSize + 8.f);
        window.draw(urlText);
    }

    // Footer hint
    sf::Text hint;
    hint.setFont(mainFont);
    hint.setCharacterSize(26);
    hint.setFillColor(sf::Color(120, 120, 140, a8));
    hint.setString(L"Bildschirm beruehren um fortzufahren");
    auto hb = hint.getLocalBounds();
    hint.setPosition((winW - hb.width) / 2.f, winH - 60.f);
    window.draw(hint);
}

// --- drawAgreement ---------------------------------------------------------
void BoothGui::drawAgreement(float alpha) {
    std::wstring title  = L"MIT Software License";
    std::wstring button = L"Accept?";
    std::vector<std::wstring> blocks;
    u_int8_t lines      = 0;
    u_int8_t margin     = 50;
    u_int8_t marginTop  = 175;
    u_int8_t lineSpacing = 40;

    float t    = stateTimer.getElapsedTime().asMilliseconds();
    float cosA = 0.3f * std::cos(t / 300.f) + 0.7f;
    auto  aa   = (uint8_t)(alpha * cosA * 255.f);

    agreementText.setFillColor(sf::Color(255, 255, 255, aa));
    agreementText.setCharacterSize(30);
    agreementText.setString(button);

    float w = agreementText.getLocalBounds().width;
    agreementText.setPosition((window.getSize().x - w) / 2.f,
                               window.getSize().y - margin - 30.f);
    window.draw(agreementText);

    agreementText.setFillColor(sf::Color(255, 255, 255, (uint8_t)(255 * alpha)));
    agreementText.setCharacterSize(50);
    agreementText.setString(title);

    w = agreementText.getLocalBounds().width;
    agreementText.setPosition((window.getSize().x - w) / 2.f, (float)margin);
    window.draw(agreementText);

    agreementText.setCharacterSize(30);
    boost::split(blocks, agreement, boost::is_any_of(L"\n"));

    for (size_t b = 0; b < blocks.size(); b++) {
        u_int8_t blocklines = 0;
        std::wstring line;
        std::vector<std::wstring> words;
        boost::split(words, blocks[b], boost::is_any_of(L" "));

        for (size_t i = 0; i - blocklines < words.size(); i++) {
            auto oldLine = line;
            line = line + words[i - blocklines] + L" ";
            agreementText.setString(line);
            float lw = agreementText.getLocalBounds().width;
            if (lw > window.getSize().x - (margin * 2)) {
                agreementText.setString(oldLine);
                agreementText.setPosition((float)margin,
                    (float)(lineSpacing * (lines + blocklines)) + marginTop);
                window.draw(agreementText);
                line.clear();
                blocklines++;
            }
        }
        agreementText.setString(line);
        agreementText.setPosition((float)margin,
            (float)(lineSpacing * (lines + blocklines)) + marginTop);
        window.draw(agreementText);

        lines += blocklines;
        lines++;
    }
}

// --- drawAlerts ------------------------------------------------------------
void BoothGui::drawAlerts() {
    if (currentState == STATE_AGREEMENT) {
        alertTimer.restart();
        return;
    }

    boost::unique_lock<boost::mutex> lk(alertMutex);

    const auto count = alerts.size();
    auto       now   = alertTimer.getElapsedTime().asMilliseconds();
    int        row   = 0;
    const int  offset_x = 35, offset_y = 20, spacing_x = 90, spacing_y = 10;
    const int  row_height = iconText.getCharacterSize() + spacing_y;

    float     alpha   = std::min(1.0f, (float)now / 300.0f);
    sf::Int32 endTime = 0;

    if (count < 1) return;

    for (auto &alert : alerts) {
        if (alert.second.endTime == 0) { endTime = 0; break; }
        if (alert.second.endTime > endTime) endTime = alert.second.endTime;
    }
    if (endTime > 0)
        alpha = std::min(alpha, std::max(0.f, (float)(endTime - now) / 300.f));

    sf::RectangleShape blackout(sf::Vector2f(window.getSize().x, window.getSize().y));
    blackout.setFillColor(sf::Color(0, 0, 0, (uint8_t)(75 * alpha)));
    window.draw(blackout);

    sf::RectangleShape background(sf::Vector2f(window.getSize().x,
                                               (row_height * count) + (offset_y * 2)));
    background.setFillColor(sf::Color(255, 255, 255, (uint8_t)(255 * alpha)));
    window.draw(background);

    vector<ALERT_TYPE> toRemove;

    for (auto &alert : alerts) {
        int y = row_height * row;
        sf::Color color = alert.second.hint ? sf::Color(75, 75, 75) : COLOR_ALERT;
        color.a = (uint8_t)(255 * alpha);

        iconText.setFillColor(color);
        alertText.setFillColor(color);
        iconText.setPosition((float)offset_x, (float)(y + offset_y + 8));
        alertText.setPosition((float)(offset_x + spacing_x), (float)(y + offset_y));
        iconText.setString(alertTypeToString.at(alert.first));
        alertText.setString(alert.second.text);
        window.draw(iconText);
        window.draw(alertText);

        if (alert.second.endTime != 0 && now >= alert.second.endTime)
            toRemove.push_back(alert.first);
        row++;
    }
    for (auto &a : toRemove) removeAlert(a, true);
}

// --- drawDebug -------------------------------------------------------------
void BoothGui::drawDebug() {
    if (!debug) return;

    sf::String s = "";
    s += "Drawing Fps:" + to_string(renderFrameCounter.fps) + "\n";
    s += "Camera Fps:"  + to_string(cameraFrameCounter.fps) + "\n";

    const auto logs = logging_sink->last_formatted();
    for (const auto &l : logs) s += l + "\n";

    debugText.setString(s);
    window.draw(debugText);
}

// --- easeOutSin ------------------------------------------------------------
float BoothGui::easeOutSin(float t, float b, float c, float d) {
    return static_cast<float>(c * std::sin(t / d * (M_PI / 2)) + b);
}

// --- showAgreement / hideAgreement -----------------------------------------
void BoothGui::showAgreement() {
    if (currentState != STATE_INIT) setState(STATE_AGREEMENT);
    else shouldShowAgreement = true;
}

void BoothGui::hideAgreement() {
    if (currentState != STATE_AGREEMENT) return;
    setState(STATE_TRANS_AGREEMENT);
}

// --- alerts ----------------------------------------------------------------
bool BoothGui::hasAlert(ALERT_TYPE type) {
    boost::unique_lock<boost::mutex> lk(alertMutex);
    return !alerts.empty() && alerts.find(type) != alerts.end();
}

void BoothGui::addAlert(ALERT_TYPE type, std::wstring text, bool autoRemove, bool isHint) {
    boost::unique_lock<boost::mutex> lk(alertMutex);
    if (alerts.empty()) alertTimer.restart();
    removeAlert(type, true);

    sf::Int32 startTime = alertTimer.getElapsedTime().asMilliseconds();
    sf::Int32 endTime   = 0;
    if (autoRemove) endTime = startTime + (isHint ? 3000 : 10000);

    alerts.insert(std::make_pair(type, (Alert){startTime, endTime, std::move(text), isHint}));
}

void BoothGui::removeAlert(ALERT_TYPE type, bool forced) {
    auto it = alerts.find(type);
    if (it == alerts.end()) return;
    if (!forced && it->second.endTime == 0)
        it->second.endTime = alertTimer.getElapsedTime().asMilliseconds() + 300;
    else
        alerts.erase(type);
}

void BoothGui::removeAlert(ALERT_TYPE type) {
    boost::unique_lock<boost::mutex> lk(alertMutex);
    removeAlert(type, false);
}

// --- printer / template ----------------------------------------------------
void BoothGui::setPrinterEnabled(bool pe)  { printerEnabled  = pe; }
void BoothGui::setTemplateEnabled(bool te) { templateEnabled = te; }

// --- cancelPrint / confirmPrint --------------------------------------------
void BoothGui::cancelPrint() {
    if (getCurrentGuiState() == STATE_FINAL_IMAGE_PRINT)
        setState(STATE_FINAL_IMAGE_PRINT_CANCELED);
}

void BoothGui::confirmPrint() {
    if (getCurrentGuiState() == STATE_FINAL_IMAGE_PRINT)
        setState(STATE_FINAL_IMAGE_PRINT_CONFIRMED);
}

void BoothGui::setDebugOutput(bool d) { debug = d; }
