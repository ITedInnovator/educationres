<?php

namespace Drupal\outside_in\Ajax;

use Drupal\Core\Ajax\OpenDialogCommand;

/**
 * Defines an AJAX command to open content in a dialog in a off-canvas tray.
 *
 * @ingroup ajax
 */
class OpenOffCanvasDialogCommand extends OpenDialogCommand {

  /**
   * Constructs an OpenOffCanvasDialogCommand object.
   *
   * Drupal provides a built-in offcanvas tray for this purpose, so no selector
   * needs to be provided.
   *
   * @param string $title
   *   The title of the dialog.
   * @param string|array $content
   *   The content that will be placed in the dialog, either a render array
   *   or an HTML string.
   * @param array $dialog_options
   *   (optional) Settings to be passed to the dialog implementation. Any
   *   jQuery UI option can be used. See http://api.jqueryui.com/dialog.
   * @param array|null $settings
   *   (optional) Custom settings that will be passed to the Drupal behaviors
   *   on the content of the dialog. If left empty, the settings will be
   *   populated automatically from the current request.
   */
  public function __construct($title, $content, array $dialog_options = [], $settings = NULL) {
    $dialog_options['modal'] = FALSE;
    parent::__construct('#drupal-offcanvas', $title, $content, $dialog_options, $settings);
  }

  /**
   * {@inheritdoc}
   */
  public function render() {
    $this->dialogOptions['modal'] = FALSE;
    return [
      'command' => 'openOffCanvas',
      'selector' => $this->selector,
      'settings' => $this->settings,
      'data' => $this->getRenderedContent(),
      'dialogOptions' => $this->dialogOptions,
    ];
  }

}
