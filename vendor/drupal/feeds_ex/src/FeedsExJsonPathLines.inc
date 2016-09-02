<?php

/**
 * @file
 * Contains FeedsExJsonPathLines.
 */

/**
 * Parses the JSON Lines format via JSONPath.
 */
class FeedsExJsonPathLines extends FeedsExJsonPath {

  /**
   * {@inheritdoc}
   */
  protected function executeContext(FeedsSource $source, FeedsFetcherResult $fetcher_result) {
    return new FeedsExLineIterator($fetcher_result->getFilePath());
  }

  /**
   * {@inheritdoc}
   */
  protected function hasConfigurableContext() {
    return FALSE;
  }

  /**
   * {@inheritdoc}
   */
  protected function setUp(FeedsSource $source, FeedsFetcherResult $fetcher_result) {
  }

  /**
   * {@inheritdoc}
   */
  protected function cleanUp(FeedsSource $source, FeedsParserResult $result) {
  }

  /**
   * {@inheritdoc}
   */
  protected function executeSourceExpression($machine_name, $expression, $row) {
    // Row is a JSON string.
    if ($encoding = $this->detectEncoding($row)) {
      $row = $this->convertEncoding($row, $encoding);
    }
    $result = jsonPath(drupal_json_decode($row), $expression);

    if (is_scalar($result)) {
      return $result;
    }

    // Return a single value if there's only one value.
    return count($result) === 1 ? $result[0] : $result;
  }

}
