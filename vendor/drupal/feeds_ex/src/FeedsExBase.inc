<?php

/**
 * @file
 * Contains FeedsExBase.
 */

/**
 * The Feeds extensible parser.
 */
abstract class FeedsExBase extends FeedsParser {

  /**
   * Whether the current system handles mb_* functions.
   *
   * @var bool
   */
  protected $isMultibyte;

  /**
   * Returns rows to be parsed.
   *
   * @param FeedsSource $source
   *   Source information.
   * @param FeedsFetcherResult $fetcher_result
   *   The result returned by the fetcher.
   *
   * @return array|Traversable
   *   Some iterable that returns rows.
   */
  abstract protected function executeContext(FeedsSource $source, FeedsFetcherResult $fetcher_result);

  /**
   * Executes a single source expression.
   *
   * @param string $machine_name
   *   The source machine name being executed.
   * @param string $expression
   *   The expression to execute.
   * @param mixed $row
   *   The row to execute on.
   *
   * @return scalar|[]scalar
   *   Either a scalar, or a list of scalars. If null, the value will be
   *   ignored.
   */
  abstract protected function executeSourceExpression($machine_name, $expression, $row);

  /**
   * Validates an expression.
   *
   * @param string &$expression
   *   The expression to validate.
   *
   * @return string|null
   *   Return the error string, or null if validation was passed.
   */
  abstract protected function validateExpression(&$expression);

  /**
   * Returns the errors after parsing.
   *
   * @return array
   *   A structured array array with keys:
   *   - message: The error message.
   *   - variables: The variables for the message.
   *   - severity: The severity of the message.
   *
   * @see watchdog()
   */
  abstract protected function getErrors();

  /**
   * Allows subclasses to prepare for parsing.
   *
   * @param FeedsSource $source
   *   The feed source.
   * @param FeedsFetcherResult $fetcher_result
   *   The result of the fetching stage.
   */
  protected function setUp(FeedsSource $source, FeedsFetcherResult $fetcher_result) {
  }

  /**
   * Allows subclasses to cleanup.
   *
   * @param FeedsSource $source
   *   The feed source.
   * @param FeedsParserResult $parser_result
   *   The result of parsing.
   */
  protected function cleanUp(FeedsSource $source, FeedsParserResult $parser_result) {
  }

  /**
   * Starts internal error handling.
   *
   * Subclasses can override this to do error handling.
   */
  protected function startErrorHandling() {
    // No default error handling.
  }

  /**
   * Stops internal error handling.
   *
   * Subclasses can override this to do error handling.
   */
  protected function stopErrorHandling() {
    // No default error handling.
  }

  /**
   * Returns whether or not this parser uses a context query.
   *
   * Sub-classes can return false here if they don't require a context query.
   *
   * @return bool
   *   True if the parser uses a context query and false if not.
   */
  protected function hasConfigurableContext() {
    return TRUE;
  }

  /**
   * {@inheritdoc}
   */
  protected function __construct($id) {
    parent::__construct($id);
    $this->isMultibyte = $GLOBALS['multibyte'] == UNICODE_MULTIBYTE;
  }

  /**
   * {@inheritdoc}
   */
  public function parse(FeedsSource $source, FeedsFetcherResult $fetcher_result) {
    $this->startErrorHandling();

    $config = $this->getConfig();
    $result = new FeedsParserResult();

    // Set link.
    $fetcher_config = $source->getConfigFor($source->importer->fetcher);
    $result->link = $fetcher_config['source'];

    $expressions = $this->prepareExpressions();
    $variable_map = $this->prepareVariables($expressions);

    try {
      $this->setUp($source, $fetcher_result);

      foreach ($this->executeContext($source, $fetcher_result) as $row) {
        if ($item = $this->executeSources($row, $expressions, $variable_map)) {
          $result->items[] = $item;
        }
      }

      $this->cleanUp($source, $result);
    }
    catch (Expcetion $exception) {
      // Do nothing. Store for later.
    }

    // Show some warnings.
    $errors = $this->getErrors();
    if ($this->config['display_errors']) {
      $this->printErrors($errors, WATCHDOG_DEBUG);
    }
    else {
      // Always print errors.
      $this->printErrors($errors);
    }
    // Log any fatal errors.
    $this->logErrors($source, $errors);
    $this->stopErrorHandling();

    if (isset($exception)) {
      throw $exception;
    }

    return $result;
  }

  /**
   * Prepares the expressions for parsing.
   *
   * At this point we just remove empty expressions.
   *
   * @return array
   *   A map of machine name to expression.
   */
  protected function prepareExpressions() {
    $expressions = array();
    foreach ($this->config['sources'] as $machine_name => $source) {
      if (strlen($source['value'])) {
        $expressions[$machine_name] = $source['value'];
      }
    }

    return $expressions;
  }

  /**
   * Prepares the variable map used to substitution.
   *
   * @param array $expressions
   *   The expressions being parsed.
   *
   * @return array
   *   A map of machine name to variable name.
   */
  protected function prepareVariables(array $expressions) {
    $variable_map = array();
    foreach ($expressions as $machine_name => $expression) {
      $variable_map[$machine_name] = '$' . $machine_name;
    }
    return $variable_map;
  }

  /**
   * Executes the source expressions.
   *
   * @param mixed $row
   *   A single item returned from the context expression.
   * @param array $expressions
   *   A map of machine name to expression.
   * @param array $variable_map
   *   A map of machine name to varible name.
   *
   * @return array
   *   The fully-parsed item array.
   */
  protected function executeSources($row, array $expressions, array $variable_map) {
    $item = array();
    $variables = array();

    foreach ($expressions as $machine_name => $expression) {
      // Variable substitution.
      $expression = strtr($expression, $variables);

      $result = $this->executeSourceExpression($machine_name, $expression, $row);

      if (!empty($this->config['sources'][$machine_name]['debug'])) {
        $this->debug($result, $machine_name);
      }

      if ($result === NULL) {
        $variables[$variable_map[$machine_name]] = '';
        continue;
      }

      $item[$machine_name] = $result;
      $variables[$variable_map[$machine_name]] = is_array($result) ? reset($result) : $result;
    }

    return $item;
  }

  /**
   * Prints errors to the screen.
   *
   * @param array $errors
   *   A list of errors as returned by stopErrorHandling().
   * @param int $severity
   *   (optional) Limit to only errors of the specified severity. Defaults to
   *   WATCHDOG_ERROR.
   *
   * @see watchdog()
   */
  protected function printErrors(array $errors, $severity = WATCHDOG_ERROR) {
    foreach ($errors as $error) {
      if ($error['severity'] > $severity) {
        continue;
      }
      drupal_set_message(t($error['message'], $error['variables']), $error['severity'] <= WATCHDOG_ERROR ? 'error' : 'warning', FALSE);
    }
  }

  /**
   * Logs errors.
   *
   * @param FeedsSource $source
   *   The feed source being importerd.
   * @param array $errors
   *   A list of errors as returned by stopErrorHandling().
   * @param int $severity
   *   (optional) Limit to only errors of the specified severity. Defaults to
   *   WATCHDOG_ERROR.
   *
   * @see watchdog()
   */
  protected function logErrors(FeedsSource $source, array $errors, $severity = WATCHDOG_ERROR) {
    foreach ($errors as $error) {
      if ($error['severity'] > $severity) {
        continue;
      }

      $source->log('feeds_ex', $error['message'], $error['variables'], $error['severity']);
    }
  }

  /**
   * Detects the encoding of a string.
   *
   * @param string $data
   *   The string to guess the encoding for.
   *
   * @return string|bool
   *   Returns the encoding, or false if one could not be detected.
   *
   * @todo Add other methods of encoding detection.
   */
  protected function detectEncoding($data) {
    if ($this->isMultibyte) {
      return mb_detect_encoding($data, $this->config['source_encoding'], TRUE);
    }
    return FALSE;
  }

  /**
   * Converts a string to UTF-8.
   *
   * Requires the iconv, GNU recode or mbstring PHP extension.
   *
   * @param string $data
   *   The string to convert.
   * @param string $encoding
   *   The encoding to convert to.
   *
   * @return string
   *   The encoded string, or the original string if encoding failed.
   *
   * @see drupal_convert_to_utf8()
   */
  protected function convertEncoding($data, $encoding) {
    $php_supported = array('utf-8', 'us-ascii', 'ascii');
    if (in_array(strtolower($encoding), $php_supported)) {
      return $data;
    }
    $converted = drupal_convert_to_utf8($data, $encoding);
    if ($converted === FALSE) {
      return $data;
    }
    return $converted;
  }

  /**
   * Renders our debug messages into a list.
   *
   * @param mixed $data
   *   The result of an expression. Either a scalar or a list of scalars.
   * @param string $machine_name
   *   The source key that produced this query.
   */
  protected function debug($data, $machine_name) {
    $name = $machine_name;
    if ($this->config['sources'][$machine_name]['name']) {
      $name = $this->config['sources'][$machine_name]['name'];
    }

    $output = '<strong>' . $name . ':</strong>';
    $data = is_array($data) ? $data : array($data);
    foreach ($data as $key => $value) {
      $data[$key] = check_plain($value);
    }
    $output .= theme('item_list', array('items' => $data));
    drupal_set_message($output);
  }

  /**
   * {@inheritdoc}
   */
  public function getMappingSources() {
    $config = $this->getConfig();
    return parent::getMappingSources() + $config['sources'];
  }

  /**
   * {@inheritdoc}
   */
  public function configDefaults() {
    return array(
      'sources' => array(),
      'context' => array(
        'value' => '',
      ),
      'display_errors' => FALSE,
      'source_encoding' => array('auto'),
      'debug_mode' => FALSE,
    );
  }

  /**
   * Reuturns the list of table headers.
   *
   * @return []string
   *   A list of header names.
   */
  protected function configFormTableHeader() {
    return array();
  }

  /**
   * Returns a form element for a specific column.
   *
   * @param array &$form_state
   *   The current form state.
   * @param string $column_name
   *   The name of the column.
   * @param array $source
   *   The individual source item.
   *
   * @return array
   *   A single form element.
   */
  protected function configFormTableColumn(array &$form_state, $column_name, array $source) {
    return array();
  }

  /**
   * {@inheritdoc}
   */
  public function configForm(&$form_state) {
    $form = array(
      '#tree' => TRUE,
      '#theme' => 'feeds_ex_configuration_table',
      '#prefix' => '<div id="feeds-ex-configuration-wrapper">',
      '#suffix' => '</div>',
    );
    $config = $this->getConfig();

    if ($this->hasConfigurableContext()) {
      $form['context']['name'] = array(
        '#type' => 'markup',
        '#markup' => t('Context'),
      );
      $form['context']['value'] = array(
        '#type' => 'textfield',
        '#title' => t('Context value'),
        '#title_display' => 'invisible',
        '#default_value' => $config['context']['value'],
        '#size' => 50,
        '#required' => TRUE,
        // We're hiding the title, so add a little hint.
        '#description' => '<span class="form-required">*</span>',
        '#attributes' => array('class' => array('feeds-ex-context-value')),
      );
    }

    $form['sources'] = array(
      '#id' => 'feeds-ex-source-table',
    );

    $max_weight = 0;
    foreach ($config['sources'] as $machine_name => $source) {
      $form['sources'][$machine_name]['name'] = array(
        '#type' => 'textfield',
        '#title' => t('Name'),
        '#title_display' => 'invisible',
        '#default_value' => $source['name'],
        '#size' => 20,
      );
      $form['sources'][$machine_name]['machine_name'] = array(
        '#title' => t('Machine name'),
        '#title_display' => 'invisible',
        '#markup' => $machine_name,
      );
      $form['sources'][$machine_name]['value'] = array(
        '#type' => 'textfield',
        '#title' => t('Value'),
        '#title_display' => 'invisible',
        '#default_value' => $source['value'],
        '#size' => 50,
      );

      foreach ($this->configFormTableHeader() as $column => $name) {
        $form['sources'][$machine_name][$column] = $this->configFormTableColumn($form_state, $column, $source);
      }

      $form['sources'][$machine_name]['debug'] = array(
        '#type' => 'checkbox',
        '#title' => t('Debug'),
        '#title_display' => 'invisible',
        '#default_value' => $source['debug'],
      );
      $form['sources'][$machine_name]['remove'] = array(
        '#type' => 'checkbox',
        '#title' => t('Remove'),
        '#title_display' => 'invisible',
      );
      $form['sources'][$machine_name]['weight'] = array(
        '#type' => 'textfield',
        '#default_value' => $source['weight'],
        '#size' => 3,
        '#attributes' => array('class' => array('feeds-ex-source-weight')),
      );
      $max_weight = $source['weight'];
    }

    $form['add']['name'] = array(
      '#type' => 'textfield',
      '#title' => t('Add new source'),
      '#id' => 'edit-sources-add-name',
      '#description' => t('Name'),
      '#size' => 20,
    );
    $form['add']['machine_name'] = array(
      '#title' => t('Machine name'),
      '#title_display' => 'invisible',
      '#type' => 'machine_name',
      '#machine_name' => array(
        'exists' => 'feeds_ex_source_exists',
        'source' => array('add', 'name'),
        'standalone' => TRUE,
        'label' => '',
      ),
      '#field_prefix' => '<span dir="ltr">',
      '#field_suffix' => '</span>&lrm;',
      '#feeds_importer' => $this->id,
      '#required' => FALSE,
      '#maxlength' => 32,
      '#size' => 15,
      '#description' => t('A unique machine-readable name containing letters, numbers, and underscores.'),
    );
    $form['add']['value'] = array(
      '#type' => 'textfield',
      '#description' => t('Value'),
      '#title' => '&nbsp;',
      '#size' => 50,
    );
    foreach ($this->configFormTableHeader() as $column => $name) {
      $form['add'][$column] = $this->configFormTableColumn($form_state, $column, array());
    }
    $form['add']['debug'] = array(
      '#type' => 'checkbox',
      '#title' => t('Debug'),
      '#title_display' => 'invisible',
    );
    $form['add']['weight'] = array(
      '#type' => 'textfield',
      '#default_value' => ++$max_weight,
      '#size' => 3,
      '#attributes' => array('class' => array('feeds-ex-source-weight')),
    );

    $form['display_errors'] = array(
      '#type' => 'checkbox',
      '#title' => t('Display errors'),
      '#description' => t('Display all error messages after parsing. Fatal errors will always be displayed.'),
      '#default_value' => $config['display_errors'],
    );
    $form['debug_mode'] = array(
      '#type' => 'checkbox',
      '#title' => t('Enable debug mode'),
      '#description' => t('Displays the configuration form on the feed source page to ease figuring out the expressions. Any values entered on that page will be saved here.'),
      '#default_value' => $config['debug_mode'],
    );

    if ($this->isMultibyte) {
      $args = array('%encodings' => implode(', ', mb_detect_order()));
      $form['source_encoding'] = array(
        '#type' => 'textfield',
        '#title' => t('Source encoding'),
        '#description' => t('The possible encodings of the source files. auto: %encodings', $args),
        '#default_value' => implode(', ', $config['source_encoding']),
        '#autocomplete_path' => '_feds_ex/encoding_autocomplete',
      );
    }

    $form['#attached']['drupal_add_tabledrag'][] = array('feeds-ex-source-table', 'order', 'sibling', 'feeds-ex-source-weight');
    $form['#attached']['css'][] = drupal_get_path('module', 'feeds_ex') . '/feeds_ex.css';

    $form['#header'] = array(
      'name' => t('Name'),
      'machine_name' => t('Machine name'),
      'value' => t('Value'),
    );
    $form['#header'] += $this->configFormTableHeader();
    $form['#header'] += array(
      'debug' => t('Debug'),
      'remove' => t('Remove'),
      'weight' => t('Weight'),
    );

    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function configFormValidate(&$values) {
    // @todo We should do this in Feeds automatically.
    $values += $this->configDefaults();
    $columns = array('name', 'value');
    $columns = array_merge($columns, array_keys($this->configFormTableHeader()));
    $columns = array_merge($columns, array('debug', 'weight'));

    // Remove sources.
    foreach ($values['sources'] as $machine_name => $source) {
      if (!empty($source['remove'])) {
        unset($values['sources'][$machine_name]);
      }
    }

    // Validate context.
    if ($message = $this->validateExpression($values['context']['value'])) {
      form_set_error('context', $message);
    }

    // Validate expressions.
    foreach (array_keys($values['sources']) as $machine_name) {
      if ($message = $this->validateExpression($values['sources'][$machine_name]['value'])) {
        form_set_error('sources][' . $machine_name . '][value', $message);
      }
    }

    // Add new source.
    if (strlen($values['add']['machine_name']) && strlen($values['add']['name'])) {
      if ($message = $this->validateExpression($values['add']['value'])) {
        form_set_error('add][value', $message);
      }
      else {
        $values['sources'][$values['add']['machine_name']] = $values['add'];
      }
    }

    // Rebuild sources to keep the configuration values clean.
    foreach ($values['sources'] as $machine_name => $source) {
      $new_value = array();
      foreach ($columns as $column) {
        $new_value[$column] = $source[$column];
      }
      $values['sources'][$machine_name] = $new_value;
    }

    // Sort by weight.
    uasort($values['sources'], 'ctools_plugin_sort');

    if (!$this->isMultibyte) {
      return;
    }
    // Normalize encodings. Make them exactly as they are defined in
    // mb_list_encodings(), but maintain user-defined order.
    $encodings = array_map('drupal_strtolower', array_map('trim', explode(',', $values['source_encoding'])));
    $values['source_encoding'] = array();
    foreach (mb_list_encodings() as $encoding) {
      // Maintain order.
      $pos = array_search(drupal_strtolower($encoding), $encodings);
      if ($pos !== FALSE) {
        $values['source_encoding'][$pos] = $encoding;
      }
    }
    ksort($values['source_encoding']);
    // Make sure there's some value set.
    if (!$values['source_encoding']) {
      $values['source_encoding'][] = 'auto';
    }
  }

  /**
   * {@inheritdoc}
   */
  public function hasConfigForm() {
    return TRUE;
  }

  /**
   * {@inheritdoc}
   */
  public function sourceDefaults() {
    return array();
  }

  /**
   * {@inheritdoc}
   */
  public function sourceForm($source_config) {
    if (!$this->hasSourceConfig()) {
      return array();
    }
    $form_state = array();
    $form = $this->configForm($form_state);
    $form['add']['machine_name']['#machine_name']['source'] = array('feeds', get_class($this), 'add', 'name');

    return $form;
  }

  /**
   * {@inheritdoc}
   */
  public function sourceFormValidate(&$source_config) {
    $this->configFormValidate($source_config);
  }

  /**
   * {@inheritdoc}
   */
  public function sourceSave(FeedsSource $source) {
    $config = $source->getConfigFor($this);
    $source->setConfigFor($this, array());

    if ($this->hasSourceConfig() && $config) {
      $this->setConfig($config);
      $this->save();
    }
  }

  /**
   * {@inheritdoc}
   */
  public function hasSourceConfig() {
    return !empty($this->config['debug_mode']);
  }

  /**
   * Sets the multibyte handling.
   *
   * @param bool $is_multibyte
   *   Whether this parser should assume multibyte handling exists.
   *
   * @internal Used only during testing to force handling.
   */
  public function setMultibyte($is_multibyte) {
    $this->isMultibyte = $is_multibyte;
  }

}

/**
 * Text lines from file iterator.
 */
class FeedsExLineIterator implements Iterator {

  /**
   * The open file handle.
   *
   * @var resource
   */
  protected $handle;

  /**
   * The current line being read.
   *
   * @var int
   */
  protected $currentLine;

  /**
   * The current position in the file.
   *
   * @var int
   */
  protected $currentPos;

  /**
   * Constructs a FeedsExLineIterator object.
   *
   * @param string $filepath
   *   The path to the file to iterate.
   */
  public function __construct($filepath) {
    $this->handle = fopen($filepath, 'r');
    $this->currentLine = NULL;
    $this->currentPos = NULL;
  }

  /**
   * Destructs a FeedsExLineIterator object.
   */
  public function __destruct() {
    if ($this->handle) {
      fclose($this->handle);
    }
  }

  /**
   * Implements Iterator::reqind().
   */
  public function rewind($pos = 0) {
    if ($this->handle) {
      fseek($this->handle, $pos);
      $this->next();
    }
  }

  /**
   * Implements Iterator::next().
   */
  public function next() {
    if ($this->handle) {
      $this->currentLine = feof($this->handle) ? NULL : fgets($this->handle);
      $this->currentPos = ftell($this->handle);
    }
  }

  /**
   * Implements Iterator::valid().
   */
  public function valid() {
    return isset($this->currentLine);
  }

  /**
   * Implements Iterator::current().
   */
  public function current() {
    return $this->currentLine;
  }

  /**
   * Implements Iterator::key().
   */
  public function key() {
    return 'line';
  }

  /**
   * Returns the current position in the file.
   *
   * @return int
   *   The current position in the file.
   */
  public function currentPos() {
    return $this->currentPos;
  }

}
