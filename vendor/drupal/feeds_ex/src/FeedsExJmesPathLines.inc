<?php

/**
 * @file
 * Contains FeedsExJmesPathLines.
 */

/**
 * Parses JSON Lines documents with JMESPath.
 */
class FeedsExJmesPathLines extends FeedsExJmesPath {

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
  protected function executeSourceExpression($machine_name, $expression, $row) {
    // Row is a JSON string.
    if ($encoding = $this->detectEncoding($row)) {
      $row = $this->convertEncoding($row, $encoding);
    }
    $row = drupal_json_decode($row);
    $result = $this->jmesPath->search($expression, $row);

    if (is_scalar($result)) {
      return $result;
    }

    // Return a single value if there's only one value.
    return count($result) === 1 ? $result[0] : $result;
  }

}
