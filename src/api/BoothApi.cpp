//
// Created by clemens on 13.02.19.
//

#include "BoothApi.h"
#include <boost/filesystem.hpp>

using namespace selfomat::api;
using namespace xtech::selfomat;
using namespace selfomat::logic;

std::string BoothApi::TAG = "API";

BoothApi::BoothApi(selfomat::logic::BoothLogic *logic, ICamera *camera, bool show_led_setup) : logic(logic),
                                                                                               camera(camera),
                                                                                               server("0.0.0.0", "9080",
                                                                                                      mux, false) {
    this->show_led_setup = show_led_setup;
}

bool BoothApi::start() {
    // Use wrapper to set needed headers
    mux.use_wrapper([this](served::response &res, const served::request &req, std::function<void()> old) {
        for (auto const &h : this->headers)
            res.set_header(h.first, h.second);

        if (req.method() == served::OPTIONS) {
            served::response::stock_reply(200, res);
        } else {
            old();
        }
    });


    mux.handle("/camera_settings/aperture")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                camera->setAperture(update.value());

                served::response::stock_reply(200, res);
            });

    mux.handle("/camera_settings/iso")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                camera->setIso(update.value());

                served::response::stock_reply(200, res);
            });

    mux.handle("/camera_settings/shutter_speed")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                camera->setShutterSpeed(update.value());

                served::response::stock_reply(200, res);
            });

    mux.handle("/camera_settings/exposure_correction")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                camera->setExposureCorrection(update.value());

                served::response::stock_reply(200, res);
            });

    mux.handle("/camera_settings/exposure_correction_trigger")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                camera->setExposureCorrectionTrigger(update.value());

                served::response::stock_reply(200, res);
            });


    mux.handle("/camera_settings/image_format")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                camera->setImageFormat(update.value());

                served::response::stock_reply(200, res);
            });

    mux.handle("/camera_settings")
            .get([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                boost::property_tree::ptree locale;
                try {
                    boost::property_tree::read_json("./i18n/" + req.header("lang") + ".json", locale);
                } catch (boost::exception &e) {
                    boost::property_tree::read_json("./i18n/en.json", locale);
                }

                CameraSettings currentCameraSettings;
                {
                    auto setting = currentCameraSettings.mutable_aperture();
                    setting->set_update_url("/camera_settings/aperture");
                    setting->set_name(locale.get<string>("api.camera.aperture"));

                    setting->set_currentindex(camera->getAperture());
                    auto *choices = camera->getApertureChoices();
                    if (choices != nullptr) {
                        for (int i = 0; i < choices->size(); i++) {
                            setting->add_values(choices->at(i));
                        }
                    }
                }

                {
                    auto setting = currentCameraSettings.mutable_iso();
                    setting->set_name("ISO");
                    setting->set_update_url("/camera_settings/iso");
                    setting->set_currentindex(camera->getIso());
                    auto *choices = camera->getIsoChoices();
                    if (choices != nullptr) {
                        for (int i = 0; i < choices->size(); i++) {
                            setting->add_values(choices->at(i));
                        }
                    }
                }

                {
                    auto setting = currentCameraSettings.mutable_shutter_speed();
                    setting->set_name(locale.get<string>("api.camera.shutterSpeed"));
                    setting->set_update_url("/camera_settings/shutter_speed");
                    setting->set_currentindex(camera->getShutterSpeed());
                    auto *choices = camera->getShutterSpeedChoices();
                    if (choices != nullptr) {
                        for (int i = 0; i < choices->size(); i++) {
                            setting->add_values(choices->at(i));
                        }
                    }
                }

                {
                    auto setting = currentCameraSettings.mutable_exposure_compensation();
                    setting->set_name(locale.get<string>("api.camera.liveBrightness"));
                    setting->set_update_url("/camera_settings/exposure_correction");
                    setting->set_currentindex(camera->getExposureCorrection());
                    auto *choices = camera->getExposureCorrectionModeChoices();
                    if (choices != nullptr) {
                        for (int i = 0; i < choices->size(); i++) {
                            setting->add_values(choices->at(i));
                        }
                    }
                }

                {
                    auto setting = currentCameraSettings.mutable_exposure_compensation_trigger();
                    setting->set_name(locale.get<string>("api.camera.captureBrightness"));
                    setting->set_update_url("/camera_settings/exposure_correction_trigger");
                    setting->set_currentindex(camera->getExposureCorrectionTrigger());
                    auto *choices = camera->getExposureCorrectionModeChoices();
                    if (choices != nullptr) {
                        for (int i = 0; i < choices->size(); i++) {
                            setting->add_values(choices->at(i));
                        }
                    }
                }

                {
                    auto setting = currentCameraSettings.mutable_image_format();
                    setting->set_name(locale.get<string>("api.camera.imageFormat"));
                    setting->set_update_url("/camera_settings/image_format");
                    setting->set_currentindex(camera->getImageFormat());
                    auto *choices = camera->getImageFormatChoices();
                    if (choices != nullptr) {
                        for (int i = 0; i < choices->size(); i++) {
                            setting->add_values(choices->at(i));
                        }
                    }
                }

                {
                    auto setting = currentCameraSettings.mutable_focus();
                    setting->set_name(locale.get<string>("api.camera.autoFocus"));
                    setting->set_post_url("/focus");
                }

                auto lensNameSetting = currentCameraSettings.mutable_lens_name();
                lensNameSetting->set_name(locale.get<string>("api.camera.lensName"));
                lensNameSetting->set_value(camera->getLensName());
                auto cameraNameSetting = currentCameraSettings.mutable_camera_name();
                cameraNameSetting->set_name(locale.get<string>("api.camera.cameraName"));
                cameraNameSetting->set_value(camera->getCameraName());


                res << currentCameraSettings.SerializeAsString();
            });

    mux.handle("/booth_settings/storage/enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setStorageEnabled(update.value(), true);

                LOG_I(TAG, "updated storage enabled to: ", std::to_string(update.value()));

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/autofocus_before_trigger/enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setAutofocusBeforeTrigger(update.value(), true);

                LOG_I(TAG, "updated autofocus before trigger enabled to: ", std::to_string(update.value()));

                served::response::stock_reply(200, res);
            });



    mux.handle("/booth_settings/flash/ittl/enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->getSelfomatController()->setFlashMode(update.value());
                logic->getSelfomatController()->commit();

                LOG_I(TAG, "updated flash mode to: ", std::to_string(update.value()));

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/printer/enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setPrinterEnabled(update.value(), true);

                LOG_I(TAG, "updated printer enabled to: ", std::to_string(update.value()));

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/print_confirmation/enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setPrintConfirmationEnabled(update.value(), true);

                LOG_I(TAG, "updated print confirmation enabled to: ", std::to_string(update.value()));

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/flash/enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setFlashEnabled(update.value(), true);

                served::response::stock_reply(200, res);
            });


    mux.handle("/booth_settings/flash/duration")
            .post([this](served::response &res, const served::request &req) {
                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                SelfomatController *controller = logic->getSelfomatController();
                controller->setFlashDurationMicros(update.value());
                controller->commit();

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/max_led_brightness")
            .post([this](served::response &res, const served::request &req) {
                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                LOG_I(TAG, "updated max led brightness to:", std::to_string(update.value()));


                SelfomatController *controller = logic->getSelfomatController();
                controller->setMaxLedBrightness(static_cast<uint8_t>(update.value()));
                controller->commit();

                served::response::stock_reply(200, res);
            });


    mux.handle("/booth_settings/template_enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                LOG_I(TAG, "updated template enabled to", std::to_string(update.value()));

                logic->setTemplateEnabled(update.value(), true);

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/debug_log_enabled")
            .post([this](served::response &res, const served::request &req) {
                BoolUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                LOG_I(TAG, "updated debug log enabled to", std::to_string(update.value()));


                logic->setDebugLogEnabled(update.value(), true);

                served::response::stock_reply(200, res);
            });


    mux.handle("/booth_settings/led_offset_cw")
            .post([this](served::response &res, const served::request &req) {

                SelfomatController *controller = logic->getSelfomatController();
                controller->moveOffsetLeft();
                served::response::stock_reply(200, res);
            });


    mux.handle("/booth_settings/led_offset_ccw")
            .post([this](served::response &res, const served::request &req) {

                SelfomatController *controller = logic->getSelfomatController();
                controller->moveOffsetRight();
                served::response::stock_reply(200, res);
            });



    mux.handle("/booth_settings/countdown_duration")
            .post([this](served::response &res, const served::request &req) {
                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                SelfomatController *controller = logic->getSelfomatController();
                controller->setCountDownMillis((update.value() + 1) * 1000);
                controller->commit();

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/led_mode")
            .post([this](served::response &res, const served::request &req) {
                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }


                SelfomatController *controller = logic->getSelfomatController();
                switch (update.value()) {
                    case 0:
                        controller->setLedType(SelfomatController::LED_TYPE::RGB.controllerValue);
                        break;
                    case 1:
                        controller->setLedType(SelfomatController::LED_TYPE::RGBW.controllerValue);
                        break;
                    default:
                        served::response::stock_reply(400, res);
                        return;
                }
                controller->commit();

                served::response::stock_reply(200, res);
            });


    mux.handle("/booth_settings/led_count")
            .post([this](served::response &res, const served::request &req) {
                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                SelfomatController *controller = logic->getSelfomatController();
                switch (update.value()) {
                    case 0:
                        controller->setLedCount(SelfomatController::LED_COUNT::COUNT_12.controllerValue);
                        break;
                    case 1:
                        controller->setLedCount(SelfomatController::LED_COUNT::COUNT_16.controllerValue);
                        break;
                    case 2:
                        controller->setLedCount(SelfomatController::LED_COUNT::COUNT_24.controllerValue);
                        break;
                    case 3:
                        controller->setLedCount(SelfomatController::LED_COUNT::COUNT_32.controllerValue);
                        break;
                    default:
                        served::response::stock_reply(400, res);
                        return;
                }
                controller->commit();

                served::response::stock_reply(200, res);
            });


    mux.handle("/trigger")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                auto controller = logic->getSelfomatController();
                controller->remoteTrigger();
                served::response::stock_reply(200, res);
            });

    mux.handle("/cancel_print")
            .post([this](served::response &res, const served::request &req) {
                logic->cancelPrint();
                served::response::stock_reply(200, res);
            });

    mux.handle("/focus")
            .post([this](served::response &res, const served::request &req) {
                if (camera->getState() != STATE_WORKING) {
                    served::response::stock_reply(503, res);
                    return;
                }

                logic->adjustFocus();
                served::response::stock_reply(200, res);
            });

    mux.handle("/flash")
            .post([this](served::response &res, const served::request &req) {

                SelfomatController *controller = logic->getSelfomatController();
                controller->triggerFlash();

                served::response::stock_reply(200, res);
            });

    mux.handle("/update")
            .post([this](served::response &res, const served::request &req) {
                logic->stopForUpdate();
                served::response::stock_reply(200, res);
            });

    mux.handle("/template_upload")
            .post([this](served::response &res, const served::request &req) {

                string body =  req.body();

                if (!logic->updateTemplate((void *)body.c_str(), body.size())) {

                    boost::property_tree::ptree locale;
                    try {
                        boost::property_tree::read_json("./i18n/" + req.header("lang") + ".json", locale);
                    } catch (boost::exception &e) {
                        boost::property_tree::read_json("./i18n/en.json", locale);
                    }

                    BoothError error;
                    error.set_title(locale.get<string>("api.error.noAlphaTitle"));
                    error.set_message(locale.get<string>("api.error.noAlpha"));
                    error.set_code(400);

                    res << error.SerializeAsString();

                    return;
                }

                served::response::stock_reply(200, res);
            });


    mux.handle("/booth_settings/language/which")
            .post([this](served::response &res, const served::request &req) {
                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setLanguageChoice(update.value(), true);

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/filter/which")
            .post([this](served::response &res, const served::request &req) {
                IntUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setFilterChoice(update.value(), true);

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings/filter/gain")
            .post([this](served::response &res, const served::request &req) {
                FloatUpdate update;
                if (!update.ParseFromString(req.body())) {
                    served::response::stock_reply(400, res);
                    return;
                }

                logic->setFilterGain(update.value(), true);

                served::response::stock_reply(200, res);
            });

    mux.handle("/booth_settings")
            .get([this](served::response &res, const served::request &req) {
                BoothSettings currentBoothSettings;

                boost::property_tree::ptree locale;
                try {
                    boost::property_tree::read_json("./i18n/" + req.header("lang") + ".json", locale);
                } catch (boost::exception &e) {
                    boost::property_tree::read_json("./i18n/en.json", locale);
                }



                auto controller = logic->getSelfomatController();

                {
                    auto setting = currentBoothSettings.mutable_language_choice();
                    setting->set_name(locale.get<string>("api.booth.languageChoice"));
                    setting->set_update_url("/booth_settings/language/which");
                    setting->set_currentindex(logic->getLanguageChoice());
                    for(auto &choice : *logic->getLanguageChoices())
                        setting->add_values(choice);
                }

                {
                    auto setting = currentBoothSettings.mutable_storage_enabled();
                    setting->set_name(locale.get<string>("api.booth.storageEnabled"));
                    setting->set_update_url("/booth_settings/storage/enabled");
                    setting->set_currentvalue(logic->getStorageEnabled());
                }

                {
                    auto setting = currentBoothSettings.mutable_printer_enabled();
                    setting->set_name(locale.get<string>("api.booth.printerEnabled"));
                    setting->set_update_url("/booth_settings/printer/enabled");
                    setting->set_currentvalue(logic->getPrinterEnabled());
                }

                {
                    auto setting = currentBoothSettings.mutable_print_confirmation_enabled();
                    setting->set_name(locale.get<string>("api.booth.printConfirmationEnabled"));
                    setting->set_update_url("/booth_settings/print_confirmation/enabled");
                    setting->set_currentvalue(logic->getPrintConfirmationEnabled());
                }

                {
                    auto setting = currentBoothSettings.mutable_filter_choice();
                    setting->set_name(locale.get<string>("api.booth.filterEnabled"));
                    setting->set_update_url("/booth_settings/filter/which");
                    setting->set_currentindex(logic->getFilterChoice());
                    for(auto &choice : *logic->getFilterChoices())
                        setting->add_values(choice);
                }

                {
                    auto setting = currentBoothSettings.mutable_filter_gain();
                    setting->set_name(locale.get<string>("api.booth.filterGain"));
                    setting->set_update_url("/booth_settings/filter/gain");
                    setting->set_currentvalue(logic->getFilterGain());
                    setting->set_minvalue(0.0);
                    setting->set_maxvalue(1.0);
                }

                {
                    auto setting = currentBoothSettings.mutable_autofocus_before_trigger();
                    setting->set_name(locale.get<string>("api.booth.autofocus_before_trigger"));
                    setting->set_update_url("/booth_settings/autofocus_before_trigger/enabled");
                    setting->set_currentvalue(logic->getAutofocusBeforeTrigger());
                }



                bool flashAvailable = logic->getFlashAvailable();
                bool flashEnabled = logic->getFlashEnabled();

                if (flashAvailable) {
                    {
                        auto setting = currentBoothSettings.mutable_flash_enabled();
                        setting->set_name(locale.get<string>("api.booth.flashEnabled"));
                        setting->set_update_url("/booth_settings/flash/enabled");
                        setting->set_currentvalue(flashEnabled);
                    }

                    {
                        auto setting = currentBoothSettings.mutable_flashmode();
                        setting->set_name(locale.get<string>("api.booth.iTTLEnabled"));
                        setting->set_update_url("/booth_settings/flash/ittl/enabled");
                        setting->set_currentvalue(logic->getSelfomatController()->getFlashMode());
                    }

                    {
                        auto setting = currentBoothSettings.mutable_flash_duration_micros();
                        setting->set_update_url("/booth_settings/flash/duration");
                        setting->set_name(locale.get<string>("api.booth.flashDuration"));
                        setting->set_currentvalue(controller->getFlashDurationMicros());
                        setting->set_minvalue(0);
                        setting->set_maxvalue(40000);
                    }

                    {
                        auto setting = currentBoothSettings.mutable_flash_test();
                        setting->set_name(locale.get<string>("api.booth.flashTest"));
                        setting->set_post_url("/flash");
                    }
                }

                {
                        auto setting = currentBoothSettings.mutable_template_upload();
                        setting->set_post_url("/template_upload");
                        setting->set_name(locale.get<string>("api.booth.templateUpload"));
                        setting->set_input_accept("image/x-png,image/png");
                }


                if (logic->getTemplateLoaded()) {
                    {
                        auto setting = currentBoothSettings.mutable_template_enabled();
                        setting->set_update_url("/booth_settings/template_enabled");
                        setting->set_name(locale.get<string>("api.booth.templateEnabled"));
                        setting->set_currentvalue(logic->getTemplateEnabled());
                    }
                }

                if (this->show_led_setup) {
                    {
                        auto setting = currentBoothSettings.mutable_led_mode();
                        setting->set_update_url("/booth_settings/led_mode");
                        setting->set_name(locale.get<string>("api.booth.ledMode"));
                        setting->set_currentindex(controller->getLedType());

                        setting->add_values(SelfomatController::LED_TYPE::RGB.humanName);
                        setting->add_values(SelfomatController::LED_TYPE::RGBW.humanName);
                    }

                    {
                        auto setting = currentBoothSettings.mutable_led_count();
                        setting->set_update_url("/booth_settings/led_count");
                        setting->set_name(locale.get<string>("api.booth.ledCount"));


                        switch (controller->getLedCount()) {
                            case 12:
                                setting->set_currentindex(0);
                                break;
                            case 16:
                                setting->set_currentindex(1);
                                break;
                            case 24:
                                setting->set_currentindex(2);
                                break;
                            case 32:
                                setting->set_currentindex(3);
                                break;
                            default:
                                setting->set_currentindex(0);
                                break;
                        }

                        setting->add_values(SelfomatController::LED_COUNT::COUNT_12.humanName);
                        setting->add_values(SelfomatController::LED_COUNT::COUNT_16.humanName);
                        setting->add_values(SelfomatController::LED_COUNT::COUNT_24.humanName);
                        setting->add_values(SelfomatController::LED_COUNT::COUNT_32.humanName);
                    }
                }


                {
                    auto setting = currentBoothSettings.mutable_led_offset_clockwise();
                    setting->set_post_url("/booth_settings/led_offset_cw");
                    setting->set_name(locale.get<string>("api.booth.ledOffset") + " +");
                }

                {
                    auto setting = currentBoothSettings.mutable_led_offset_counter_clockwise();
                    setting->set_post_url("/booth_settings/led_offset_ccw");
                    setting->set_name(locale.get<string>("api.booth.ledOffset") + " -");
                }


                {
                    auto setting = currentBoothSettings.mutable_countdown_duration();
                    setting->set_update_url("/booth_settings/countdown_duration");
                    setting->set_name(locale.get<string>("api.booth.countdownDuration"));

                    setting->set_currentindex(controller->getCountDownMillis() / 1000 - 1);

                    for (int i = 1; i <= 10; i++) {
                        setting->add_values(std::to_string(i) + "s");
                    }
                }

                {
                    auto setting = currentBoothSettings.mutable_max_led_brightness();
                    setting->set_update_url("/booth_settings/max_led_brightness");
                    setting->set_name(locale.get<string>("api.booth.maxLedBrightness"));
                    setting->set_minvalue(20);
                    setting->set_maxvalue(255);
                    setting->set_currentvalue(controller->getMaxLedBrightness());
                }


                {
                    auto setting = currentBoothSettings.mutable_update_mode();
                    setting->set_name(locale.get<string>("api.booth.updateMode"));
                    setting->set_post_url("/update");
                    setting->set_alert(locale.get<string>("api.booth.updateAlert"));
                }

                {
                    auto setting = currentBoothSettings.mutable_cups_link();
                    setting->set_name(locale.get<string>("api.booth.cupsSetup"));
                    setting->set_url("http://192.168.4.1:631");
                }

                {
                    auto setting = currentBoothSettings.mutable_debug_log_enabled();
                    setting->set_update_url("/booth_settings/debug_log_enabled");
                    setting->set_name(locale.get<string>("api.booth.debugLogEnabled"));
                    setting->set_currentvalue(logic->getDebugLogEnabled());
                }

                {
                    auto triggerCountSetting = currentBoothSettings.mutable_trigger_counter();
                    triggerCountSetting->set_name(locale.get<string>("api.booth.triggerCounter"));
                    triggerCountSetting->set_value(std::to_string(logic->getTriggerCounter()));
                }

                {
                    auto setting = currentBoothSettings.mutable_software_version();
                    setting->set_name(locale.get<string>("api.booth.software_version"));
                    setting->set_value("unreleased (Build date + time: " __DATE__ " " __TIME__ ")");
                }


                res << currentBoothSettings.SerializeAsString();
            });





    mux.handle("/stress")
            .post([this](served::response &res, const served::request &req) {
                auto controller = logic->getSelfomatController();
                controller->setStressTestEnabled(true);
                served::response::stock_reply(200, res);
            });

    mux.handle("/unstress")
            .post([this](served::response &res, const served::request &req) {
                auto controller = logic->getSelfomatController();
                controller->setStressTestEnabled(false);
                served::response::stock_reply(200, res);
            });

    mux.handle("/version")
            .get([this](served::response &res, const served::request &req) {
                std::string filename = "./version";

                ifstream f(filename, ios::in);
                string file_contents{istreambuf_iterator<char>(f), istreambuf_iterator<char>()};

                res.set_status(200);
                res.set_body(file_contents);
            });

    mux.handle("/app/assets/i18n/{file}")
            .get([this](served::response &res, const served::request &req) {
                std::string filename = "./app/assets/i18n/" + req.params["file"];

                res.set_header("Content-Type", "application/json");

                ifstream f(filename, ios::in);
                string file_contents{istreambuf_iterator<char>(f), istreambuf_iterator<char>()};

                res.set_status(200);
                res.set_body(file_contents);
            });


    mux.handle("/app/svg/{file}")
            .get([this](served::response &res, const served::request &req) {
                std::string filename = "./app/svg/" + req.params["file"];

                res.set_header("Content-Type", "image/svg+xml");

                ifstream f(filename, ios::in);
                string file_contents{istreambuf_iterator<char>(f), istreambuf_iterator<char>()};

                res.set_status(200);
                res.set_body(file_contents);
            });

    mux.handle("/app/{file}")
            .get([this](served::response &res, const served::request &req) {
                string file = req.params["file"];

                if (file.compare("tabs") == 0) {
                    res.set_status(301);
                    res.set_header("Location", "/app/index.html");
                } else {
                    // Determine Content-Type from file extension
                    std::string contentType = "application/octet-stream";
                    auto dotPos = file.rfind('.');
                    if (dotPos != std::string::npos) {
                        std::string ext = file.substr(dotPos);
                        if (ext == ".js")        contentType = "application/javascript";
                        else if (ext == ".css")   contentType = "text/css";
                        else if (ext == ".html")  contentType = "text/html";
                        else if (ext == ".json")  contentType = "application/json";
                        else if (ext == ".png")   contentType = "image/png";
                        else if (ext == ".svg")   contentType = "image/svg+xml";
                        else if (ext == ".jpg" || ext == ".jpeg") contentType = "image/jpeg";
                        else if (ext == ".woff")  contentType = "font/woff";
                        else if (ext == ".woff2") contentType = "font/woff2";
                        else if (ext == ".ttf")   contentType = "font/ttf";
                        else if (ext == ".ico")   contentType = "image/x-icon";
                    }
                    res.set_header("Content-Type", contentType);

                    ifstream f("./app/" + file, ios::in | ios::binary);
                    string file_contents{istreambuf_iterator<char>(f), istreambuf_iterator<char>()};

                    res.set_status(200);
                    res.set_body(file_contents);
                }
        });

    mux.handle("/{file}")
            .get([this](served::response &res, const served::request &req) {
                string file = req.params["file"];

                if (file.compare("app") == 0) {
                    res.set_status(301);
                    res.set_header("Location", "/app/index.html");
                }
            });

    // ── Photo gallery endpoints ────────────────────────────────────────────
    // GET /gallery  →  simple HTML download page
    mux.handle("/gallery")
            .get([this](served::response &res, const served::request &req) {
                res.set_header("Content-Type", "text/html; charset=UTF-8");
                res.set_status(200);
                res.set_body(R"html(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>FotoBox – Fotos herunterladen</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:linear-gradient(135deg,#0e0e16 0%,#1a1a2e 100%);color:#eee;font-family:'Segoe UI',system-ui,sans-serif;padding:20px;min-height:100vh}
  h1{text-align:center;margin:24px 0 8px;font-size:2rem;color:#fff;font-weight:700;letter-spacing:-.5px}
  .subtitle{text-align:center;color:#64d2b4;font-size:1rem;margin-bottom:28px;font-weight:500}
  .grid{display:flex;flex-wrap:wrap;gap:16px;justify-content:center;padding:0 8px}
  .card{background:rgba(30,30,46,.85);border-radius:16px;overflow:hidden;width:200px;text-align:center;
        transition:transform .2s,box-shadow .2s;border:1px solid rgba(100,210,180,.1);
        box-shadow:0 4px 20px rgba(0,0,0,.3)}
  .card:hover{transform:translateY(-4px);box-shadow:0 8px 32px rgba(100,210,180,.15)}
  .card img{width:100%;aspect-ratio:4/3;object-fit:cover;display:block;background:#111}
  .card a{display:block;padding:10px 12px;color:#64d2b4;text-decoration:none;font-size:.78rem;
          word-break:break-all;font-weight:500;transition:color .2s}
  .card a:hover{color:#fff}
  #msg{text-align:center;margin-top:60px;color:#666;font-size:1.1rem}
  .top-bar{width:100%;height:4px;background:linear-gradient(90deg,#64d2b4,#3ea889);border-radius:2px;margin-bottom:8px}
  .dl-all{display:block;margin:0 auto 24px;padding:12px 32px;background:#28a882;color:#fff;
          border:none;border-radius:12px;font-size:1rem;cursor:pointer;font-weight:600;
          transition:background .2s}
  .dl-all:hover{background:#33c79a}
</style>
</head>
<body>
<div class="top-bar"></div>
<h1>FotoBox</h1>
<p class="subtitle">Deine Fotos zum Herunterladen</p>
<div class="grid" id="grid"></div>
<p id="msg">Lade Fotos&#8230;</p>
<script>
fetch('/photos')
  .then(function(r){return r.json()})
  .then(function(files){
    var grid=document.getElementById('grid');
    var msg=document.getElementById('msg');
    if(!files||files.length===0){msg.textContent='Noch keine Fotos vorhanden.';return}
    msg.textContent='';
    files.forEach(function(f){
      var card=document.createElement('div');
      card.className='card';
      card.innerHTML='<a href="/photos/'+encodeURIComponent(f)+'" download><img src="/photos/'+encodeURIComponent(f)+'" loading="lazy" alt="'+f+'"><span>'+f+'</span></a>';
      grid.appendChild(card);
    });
  })
  .catch(function(){document.getElementById('msg').textContent='Fehler beim Laden.';});
</script>
</body>
</html>)html");
            });

    // GET /photos  →  JSON array of filenames
    mux.handle("/photos")
            .get([this](served::response &res, const served::request &req) {
                std::string dir = logic->getImageDir();
                res.set_header("Content-Type", "application/json");
                if (dir.empty() || !boost::filesystem::exists(dir)) {
                    res.set_status(200);
                    res.set_body("[]");
                    return;
                }
                // Helper to JSON-escape a string
                auto jsonEscape = [](const std::string &s) {
                    std::string out;
                    out.reserve(s.size() + 4);
                    for (char c : s) {
                        if (c == '"' || c == '\\') out += '\\';
                        out += c;
                    }
                    return out;
                };
                std::string json = "[";
                bool first = true;
                try {
                    for (auto &entry : boost::filesystem::directory_iterator(dir)) {
                        auto ext = entry.path().extension().string();
                        if (ext == ".jpg" || ext == ".JPG" || ext == ".jpeg" || ext == ".JPEG"
                            || ext == ".png" || ext == ".PNG") {
                            if (!first) json += ",";
                            std::string name = entry.path().filename().string();
                            json += "\"" + jsonEscape(name) + "\"";
                            first = false;
                        }
                    }
                } catch (...) {}
                json += "]";
                res.set_status(200);
                res.set_body(json);
            });

    // GET /photos/{file}  →  serve a single photo
    mux.handle("/photos/{file}")
            .get([this](served::response &res, const served::request &req) {
                std::string dir = logic->getImageDir();
                std::string filename = req.params["file"];
                // Path-traversal guard: resolve canonical path and ensure it stays within imageDir
                boost::filesystem::path resolvedDir = boost::filesystem::canonical(dir);
                boost::filesystem::path candidate   = boost::filesystem::absolute(
                    boost::filesystem::path(dir) / filename);
                try {
                    candidate = boost::filesystem::canonical(candidate);
                } catch (...) {
                    served::response::stock_reply(404, res);
                    return;
                }
                // Ensure resolved path starts with the image directory
                auto dirStr = resolvedDir.string() + "/";
                auto candStr = candidate.string();
                if (candStr.substr(0, dirStr.size()) != dirStr) {
                    served::response::stock_reply(403, res);
                    return;
                }
                ifstream f(candidate.string(), ios::binary | ios::in);
                if (!f.is_open()) {
                    served::response::stock_reply(404, res);
                    return;
                }
                std::string ext = candidate.extension().string();
                std::string ct = "image/jpeg";
                if (ext == ".png" || ext == ".PNG") ct = "image/png";
                res.set_header("Content-Type", ct);
                res.set_header("Content-Disposition",
                               "attachment; filename=\"" + candidate.filename().string() + "\"");
                string body{istreambuf_iterator<char>(f), istreambuf_iterator<char>()};
                res.set_status(200);
                res.set_body(body);
            });

    // Create the server and run with 2 handler thread.
    // THIS IS NEEDED BECAUSE BLOCKING=FALSE IS IGNORED BY SERVERD IF THREADS = 1!!!!
    server.run(2, false);


    LOG_I(TAG, "API started!");


    return true;
}

bool BoothApi::stop() {
    LOG_I(TAG, "Stopping the API");

    server.stop();

    return true;
}
