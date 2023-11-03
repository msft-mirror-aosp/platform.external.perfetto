// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import m from 'mithril';

import {classNames} from '../base/classnames';

import {HTMLButtonAttrs} from './common';
import {Icon} from './icon';
import {Popup} from './popup';

interface CommonAttrs extends HTMLButtonAttrs {
  // Always show the button as if the "active" pseudo class were applied, which
  // makes the button look permanently pressed.
  // Useful for when the button represents some toggleable state, such as
  // showing/hiding a popup menu.
  // Defaults to false.
  active?: boolean;
  // Use minimal padding, reducing the overall size of the button by a few px.
  // Defaults to false.
  compact?: boolean;
  // Reduces button decorations.
  // Defaults to false.
  minimal?: boolean;
  // Optional right icon.
  rightIcon?: string;
  // List of space separated class names forwarded to the icon.
  className?: string;
  // Allow clicking this button to close parent popups.
  // Defaults to false.
  dismissPopup?: boolean;
}

interface IconButtonAttrs extends CommonAttrs {
  // Icon buttons require an icon.
  icon: string;
}

interface LabelButtonAttrs extends CommonAttrs {
  // Label buttons require a label.
  label: string;
  // Label buttons can have an optional icon.
  icon?: string;
}

export type ButtonAttrs = LabelButtonAttrs|IconButtonAttrs;

export class Button implements m.ClassComponent<ButtonAttrs> {
  view({attrs}: m.CVnode<ButtonAttrs>) {
    const {
      icon,
      active,
      compact,
      minimal,
      rightIcon,
      className,
      dismissPopup,
      ...htmlAttrs
    } = attrs;

    const label = 'label' in attrs ? attrs.label : undefined;

    const classes = classNames(
        active && 'pf-active',
        compact && 'pf-compact',
        minimal && 'pf-minimal',
        (icon && !label) && 'pf-icon-only',
        dismissPopup && Popup.DISMISS_POPUP_GROUP_CLASS,
        className,
    );

    return m(
        'button.pf-button',
        {
          ...htmlAttrs,
          className: classes,
        },
        icon && m(Icon, {className: 'pf-left-icon', icon}),
        rightIcon && m(Icon, {className: 'pf-right-icon', icon: rightIcon}),
        label || '\u200B',  // Zero width space keeps button in-flow
    );
  }
}
