/*
 * SPDX-FileCopyrightText: 2024~2024
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_RIMEVOICEDEF_H_
#define _FCITX_RIMEVOICEDEF_H_

#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/i18n.h>

namespace fcitx::rime {

enum class VoiceModifierKey { None, Shift, Ctrl, Alt, Super };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(VoiceModifierKey, N_("None"), N_("Shift"),
                                 N_("Ctrl"), N_("Alt"), N_("Super"));


} // namespace fcitx::rime

#endif
