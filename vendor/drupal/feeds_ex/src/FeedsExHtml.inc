<?php

/**
 * @file
 * Contains FeedsExHtml.
 */

/**
 * Parses HTML documents with XPath.
 */
class FeedsExHtml extends FeedsExXml {

  /**
   * Whether this version of PHP has the correct saveHTML() method.
   *
   * @var bool
   */
  protected $useSaveHTML;

  /**
   * {@inheritdoc}
   */
  public function __construct($id) {
    parent::__construct($id);
    // DOMDocument::saveHTML() cannot take $node as an argument prior to 5.3.6.
    $this->useSaveHTML = version_compare(PHP_VERSION, '5.3.6', '>=');
  }

  /**
   * {@inheritdoc}
   */
  protected function convertEncoding($data, $encoding = 'UTF-8') {
    // Check for an encoding declaration.
    $matches = FALSE;
    if (preg_match('/<meta[^>]+charset\s*=\s*["\']?([\w-]+)\b/i', $data, $matches)) {
      $encoding = $matches[1];
    }
    elseif ($detected = parent::detectEncoding($data)) {
      $encoding = $detected;
    }

    // Unsupported encodings are converted here into UTF-8.
    $php_supported = array('utf-8', 'us-ascii', 'ascii');
    if (in_array(strtolower($encoding), $php_supported)) {
      return $data;
    }

    $data = parent::convertEncoding($data, $encoding);
    if ($matches) {
      $data = preg_replace('/(<meta[^>]+charset\s*=\s*["\']?)([\w-]+)\b/i', '$1UTF-8', $data, 1);
    }

    return $data;
  }

  /**
   * {@inheritdoc}
   */
  protected function prepareDocument(FeedsSource $source, FeedsFetcherResult $fetcher_result) {
    $document = new DOMDocument('1.0', 'UTF-8');
    $document->strictErrorChecking = FALSE;
    $document->resolveExternals = FALSE;
    // Libxml specific.
    $document->substituteEntities = FALSE;
    $document->recover = TRUE;
    $document->encoding = 'UTF-8';

    // Convert encoding.
    $raw = $this->convertEncoding($fetcher_result->getRaw());
    // Fun hack to force parsing as utf-8.
    $raw = '<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />' . "\n" . $raw;
    // Pass in options if available.
    if (version_compare(PHP_VERSION, '5.4.0', '>=')) {
      $options = LIBXML_NOENT | LIBXML_NONET | defined('LIBXML_COMPACT') ? LIBXML_COMPACT : 0;
      $success = $document->loadHTML($raw, $options);
    }
    else {
      $success = $document->loadHTML($raw);
    }

    if (!$success) {
      throw new RuntimeException(t('There was an error parsing the HTML document.'));
    }

    return $document;
  }

  /**
   * {@inheritdoc}
   */
  protected function getRaw(DOMNode $node) {
    if ($this->useSaveHTML) {
      return $node->ownerDocument->saveHTML($node);
    }

    return $node->ownerDocument->saveXML($node, LIBXML_NOEMPTYTAG);
  }

}
