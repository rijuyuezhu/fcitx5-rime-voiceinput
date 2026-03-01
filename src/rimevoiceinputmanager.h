/*
 * SPDX-FileCopyrightText: 2024~2024
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_RIMEVOICEINPUTMANAGER_H_
#define _FCITX_RIMEVOICEINPUTMANAGER_H_

#include "rimevoicedef.h"
#include <fcitx-utils/event.h>
#include <fcitx-utils/key.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <optional>
#include <string>

namespace fcitx::rime {

class RimeEngine;

enum class VoiceInputState { Idle, Recording, Processing };
enum class VoiceModifier { None, M1, M2, M1_M2 };

class VoiceInputManager {
public:
    VoiceInputManager(RimeEngine *engine);

    void loadConfig();
    bool toggleRecording(InputContext *ic, const KeyEvent &event, bool isEdit);
    bool startRecording(InputContext *ic, const KeyEvent &event, bool isEdit);
    bool stopRecording(InputContext *ic);
    void clear(InputContext *ic);
    void reset(InputContext *ic);
    VoiceInputState getState() const { return state_; }
    VoiceModifier getCurrentModifier() const { return currentModifier_; }

private:
    VoiceModifier detectModifier(const Key &key) const;
    KeyState modifierKeyToState(VoiceModifierKey key) const;
    bool executeCommand(const std::string &command);
    void selectCommands(VoiceModifier modifier, bool isEdit);
    void fetchResult(InputContext *ic);
    std::string readFromFile();
    void showStatus(InputContext *ic, const std::string &text);
    void clearStatus(InputContext *ic);
    std::string modifierToString(VoiceModifier modifier) const;
    void storeEditText(InputContext *ic);

    RimeEngine *engine_;
    VoiceInputState state_;
    VoiceModifier currentModifier_;

    struct CommandPair {
        const std::string *start;
        const std::string *stop;
        const std::string *cancel;
    };
    std::optional<CommandPair> currentCommands_;

    std::string resultPath_;
    std::string editTextStorePath_;
    std::string recordingText_;
    std::string editRecordingText_;
    std::string processingText_;

    VoiceModifierKey configM1_;
    VoiceModifierKey configM2_;
    KeyState m1State_;
    KeyState m2State_;
};

} // namespace fcitx::rime

#endif
