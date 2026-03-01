/*
 * SPDX-FileCopyrightText: 2024~2024
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "rimevoiceinputmanager.h"
#include "rimeengine.h"
#include "rimestate.h"
#include <fcitx-utils/key.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fcitx::rime {

#define VOICE_DEBUG() FCITX_LOGC(rime_log, Debug)
#define VOICE_ERROR() FCITX_LOGC(rime_log, Error)

VoiceInputManager::VoiceInputManager(RimeEngine *engine)
    : engine_(engine), state_(VoiceInputState::Idle),
      currentModifier_(VoiceModifier::None), configM1_(VoiceModifierKey::Shift),
      configM2_(VoiceModifierKey::Ctrl), m1State_(KeyState::Shift),
      m2State_(KeyState::Ctrl) {
    loadConfig();
}

void VoiceInputManager::loadConfig() {
    const auto &config = engine_->config();

    configM1_ = config.VoiceModifier_M1.value();
    configM2_ = config.VoiceModifier_M2.value();
    m1State_ = modifierKeyToState(configM1_);
    m2State_ = modifierKeyToState(configM2_);
    resultPath_ = config.voiceResultPath.value();
    recordingText_ = config.voiceRecordingText.value();
    editTextStorePath_ = config.voiceEditTextStorePath.value();
    processingText_ = config.voiceProcessingText.value();
}

KeyState VoiceInputManager::modifierKeyToState(VoiceModifierKey key) const {
    switch (key) {
    case VoiceModifierKey::Shift:
        return KeyState::Shift;
    case VoiceModifierKey::Ctrl:
        return KeyState::Ctrl;
    case VoiceModifierKey::Alt:
        return KeyState::Alt;
    case VoiceModifierKey::Super:
        return KeyState::Super;
    default:
        return KeyState::NoState;
    }
}

VoiceModifier VoiceInputManager::detectModifier(const Key &key) const {
    bool hasM1 = false;
    bool hasM2 = false;

    if (configM1_ != VoiceModifierKey::None) {
        hasM1 = key.states().test(m1State_);
    }

    if (configM2_ != VoiceModifierKey::None) {
        hasM2 = key.states().test(m2State_);
    }

    if (hasM1 && hasM2) {
        return VoiceModifier::M1_M2;
    } else if (hasM1) {
        return VoiceModifier::M1;
    } else if (hasM2) {
        return VoiceModifier::M2;
    } else {
        return VoiceModifier::None;
    }
}

std::string VoiceInputManager::modifierToString(VoiceModifier modifier) const {
    switch (modifier) {
    case VoiceModifier::None:
        return "";
    case VoiceModifier::M1:
        switch (configM1_) {
        case VoiceModifierKey::Shift:
            return "Shift";
        case VoiceModifierKey::Ctrl:
            return "Ctrl";
        case VoiceModifierKey::Alt:
            return "Alt";
        case VoiceModifierKey::Super:
            return "Super";
        default:
            return "M1";
        }
    case VoiceModifier::M2:
        switch (configM2_) {
        case VoiceModifierKey::Shift:
            return "Shift";
        case VoiceModifierKey::Ctrl:
            return "Ctrl";
        case VoiceModifierKey::Alt:
            return "Alt";
        case VoiceModifierKey::Super:
            return "Super";
        default:
            return "M2";
        }
    case VoiceModifier::M1_M2: {
        std::string m1, m2;
        switch (configM1_) {
        case VoiceModifierKey::Shift:
            m1 = "Shift";
            break;
        case VoiceModifierKey::Ctrl:
            m1 = "Ctrl";
            break;
        case VoiceModifierKey::Alt:
            m1 = "Alt";
            break;
        case VoiceModifierKey::Super:
            m1 = "Super";
            break;
        default:
            m1 = "M1";
        }
        switch (configM2_) {
        case VoiceModifierKey::Shift:
            m2 = "Shift";
            break;
        case VoiceModifierKey::Ctrl:
            m2 = "Ctrl";
            break;
        case VoiceModifierKey::Alt:
            m2 = "Alt";
            break;
        case VoiceModifierKey::Super:
            m2 = "Super";
            break;
        default:
            m2 = "M2";
        }
        return m1 + "+" + m2;
    }
    }
    return "";
}

void VoiceInputManager::selectCommands(VoiceModifier modifier, bool isEdit) {
    const auto &config = engine_->config();

    CommandPair commands;
    if (isEdit) {
        switch (modifier) {
        case VoiceModifier::None:
            commands.start = &config.voiceEditCommandStart.value();
            commands.stop = &config.voiceEditCommandStop.value();
            commands.cancel = &config.voiceEditCommandCancel.value();
            break;
        case VoiceModifier::M1:
            commands.start = &config.voiceEditCommandM1Start.value();
            commands.stop = &config.voiceEditCommandM1Stop.value();
            commands.cancel = &config.voiceEditCommandM1Cancel.value();
            break;
        case VoiceModifier::M2:
            commands.start = &config.voiceEditCommandM2Start.value();
            commands.stop = &config.voiceEditCommandM2Stop.value();
            commands.cancel = &config.voiceEditCommandM2Cancel.value();
            break;
        case VoiceModifier::M1_M2:
            commands.start = &config.voiceEditCommandM1M2Start.value();
            commands.stop = &config.voiceEditCommandM1M2Stop.value();
            commands.cancel = &config.voiceEditCommandM1M2Cancel.value();
            break;
        }
    } else {
        switch (modifier) {
        case VoiceModifier::None:
            commands.start = &config.voiceCommandStart.value();
            commands.stop = &config.voiceCommandStop.value();
            commands.cancel = &config.voiceCommandCancel.value();
            break;
        case VoiceModifier::M1:
            commands.start = &config.voiceCommandM1Start.value();
            commands.stop = &config.voiceCommandM1Stop.value();
            commands.cancel = &config.voiceCommandM1Cancel.value();
            break;
        case VoiceModifier::M2:
            commands.start = &config.voiceCommandM2Start.value();
            commands.stop = &config.voiceCommandM2Stop.value();
            commands.cancel = &config.voiceCommandM2Cancel.value();
            break;
        case VoiceModifier::M1_M2:
            commands.start = &config.voiceCommandM1M2Start.value();
            commands.stop = &config.voiceCommandM1M2Stop.value();
            commands.cancel = &config.voiceCommandM1M2Cancel.value();
            break;
        }
    }

    currentCommands_ = commands;
}

bool VoiceInputManager::toggleRecording(InputContext *ic, const KeyEvent &key,
                                        bool isEdit) {
    switch (state_) {
    case VoiceInputState::Idle:
        return startRecording(ic, key, isEdit);
    case VoiceInputState::Recording:
        return stopRecording(ic);
    case VoiceInputState::Processing:
        VOICE_DEBUG() << "Already processing, ignoring";
        return false;
    }
    return false;
}

bool VoiceInputManager::startRecording(InputContext *ic, const KeyEvent &event,
                                       bool isEdit) {
    currentModifier_ = detectModifier(event.key());
    VOICE_DEBUG() << "Detected modifier: "
                  << modifierToString(currentModifier_);

    selectCommands(currentModifier_, isEdit);
    if (!currentCommands_) {
        VOICE_ERROR() << "No command configured for modifier: "
                      << modifierToString(currentModifier_);
        return false;
    }

    VOICE_DEBUG() << "Starting recording with modifier: "
                  << modifierToString(currentModifier_)
                  << ", command: " << *currentCommands_->start;

    if (isEdit) {
        storeEditText(ic);
    }

    if (!executeCommand(*currentCommands_->start)) {
        VOICE_ERROR() << "Failed to execute start command";
        return false;
    }

    state_ = VoiceInputState::Recording;

    std::string displayText = isEdit ? editRecordingText_ : recordingText_;
    if (!modifierToString(currentModifier_).empty()) {
        displayText += " [" + modifierToString(currentModifier_) + "]";
    }
    showStatus(ic, displayText);

    return true;
}

bool VoiceInputManager::stopRecording(InputContext *ic) {
    VOICE_DEBUG() << "Stopping recording (modifier: "
                  << modifierToString(currentModifier_) << ")";

    if (!currentCommands_) {
        VOICE_ERROR() << "No command configured for modifier: "
                      << modifierToString(currentModifier_);
        clearStatus(ic);
        state_ = VoiceInputState::Idle;
        return false;
    }
    clearStatus(ic);
    state_ = VoiceInputState::Processing;
    showStatus(ic, processingText_);
    engine_->instance()->eventDispatcher().schedule([this, ic]() {
        if (executeCommand(*currentCommands_->stop)) {
            fetchResult(ic);
        } else {
            VOICE_ERROR() << "Failed to execute stop command";
        }
        clear(ic);
    });

    return true;
}

void VoiceInputManager::clear(InputContext *ic) {
    clearStatus(ic);

    state_ = VoiceInputState::Idle;
    currentModifier_ = VoiceModifier::None;
    currentCommands_.reset();
}

void VoiceInputManager::reset(InputContext *ic) {
    if (state_ == VoiceInputState::Idle) {
        return;
    }

    VOICE_DEBUG() << "Resetting voice input manager (current state: "
                  << (state_ == VoiceInputState::Recording ? "Recording"
                                                           : "Processing")
                  << ")";

    if (state_ == VoiceInputState::Recording && currentCommands_) {
        VOICE_DEBUG() << "Stopping recording due to reset";
        executeCommand(*currentCommands_->cancel);
    }

    clear(ic);

    return;
}

bool VoiceInputManager::executeCommand(const std::string &command) {
    VOICE_DEBUG() << "Executing command: " << command;

    pid_t pid = fork();
    if (pid == -1) {
        VOICE_ERROR() << "Failed to fork process";
        return false;
    }

    if (pid == 0) {
        // Child process
        // Redirect stdout to /dev/null
        int fd = open("/dev/null", O_WRONLY);
        if (fd != -1) {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Execute command through shell (search from PATH)
        execlp("sh", "sh", "-c", command.c_str(), nullptr);

        // If execlp returns, it failed
        _exit(127);
    }

    // Parent process: wait for child
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        VOICE_ERROR() << "waitpid failed";
        return false;
    }

    VOICE_DEBUG() << "Command executed, status: " << status;

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void VoiceInputManager::fetchResult(InputContext *ic) {
    std::string result;

    try {
        result = readFromFile();
    } catch (const std::exception &e) {
        VOICE_ERROR() << "Exception while fetching result: " << e.what();
    }

    clearStatus(ic);

    if (!result.empty()) {
        VOICE_DEBUG() << "Voice result: " << result;

        if (auto *state = engine_->state(ic)) {
            state->commitVoiceText(result);
        }
    } else {
        VOICE_DEBUG() << "Empty result";
    }
}

std::string VoiceInputManager::readFromFile() {
    std::ifstream file(resultPath_);
    if (!file.is_open()) {
        VOICE_ERROR() << "Failed to open file: " << resultPath_;
        return "";
    }

    std::string result((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    std::ofstream clearFile(resultPath_, std::ios::trunc);
    clearFile.close();

    while (!result.empty() &&
           (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

void VoiceInputManager::showStatus(InputContext *ic, const std::string &text) {
    VOICE_DEBUG() << "Showing status: " << text;
    auto &inputPanel = ic->inputPanel();
    inputPanel.setAuxUp(Text(text));
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void VoiceInputManager::clearStatus(InputContext *ic) {
    VOICE_DEBUG() << "Clearing status";
    auto &inputPanel = ic->inputPanel();
    inputPanel.setAuxUp(Text());
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void VoiceInputManager::storeEditText(InputContext *ic) {
    std::string editText = engine_->getEditText(ic);
    VOICE_DEBUG() << "Edit text: " << editText;
    std::ofstream storeFile(editTextStorePath_);
    if (storeFile.is_open()) {
        storeFile << editText;
        storeFile.close();
    } else {
        VOICE_ERROR() << "Failed to open edit text store file: " << editTextStorePath_;
    }
}

} // namespace fcitx::rime