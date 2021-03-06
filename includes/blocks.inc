<?php

/**
 * @file
 * Contains some block related functionality.
 */

use Drupal\Core\Url;
use Drupal\Core\Template\Attribute;
use Drupal\Component\Utility\Unicode;
use Drupal\Component\Utility\Html;
use Drupal\Core\Form\FormStateInterface;
use Drupal\Core\Cache\CacheableMetadata;

use Drupal\islandora_solr\IslandoraSolrQueryProcessor;

/**
 * Ajax Callback to reload the facet form when adding rows to the table.
 */
function _islandora_solr_update_filter_table($form, FormStateInterface $form_state) {
  return $form['settings']['facet'];
}

/**
 * AJAX callback for advanced search form.
 *
 * @param array $form
 *   An associative array containing form structure.
 * @param \Drupal\Core\Form\FormStateInterface $form_state
 *   An associative array containing form state.
 *
 * @return array
 *   An associative array containing the fully built form element.
 *
 * @see islandora_solr_advanced_search_form()
 */
function _islandora_solr_advanced_search_terms(array $form, FormStateInterface $form_state) {
  return $form['terms'];
}

/**
 * Provides list to change primary displays.
 *
 * Only populated with displays that are enabled through the admin form.
 *
 * @global IslandoraSolrQueryProcessor $_islandora_solr_queryclass
 *   The IslandoraSolrQueryProcessor object which includes the current query
 *   settings and the raw Solr results.
 *
 * @return array
 *   A render array list containing enabled primary display links.
 */
function islandora_solr_display() {
  global $_islandora_solr_queryclass;

  $cache_meta = (new CacheableMetadata())
    ->addCacheContexts([
      'url',
    ]);

  $output = [];

  // Check if the page is dealing with search results.
  if (islandora_solr_results_page($_islandora_solr_queryclass)) {
    $cache_meta->addCacheableDependency($_islandora_solr_queryclass);

    // Parameters set in URL.
    $params = isset($_islandora_solr_queryclass->internalSolrParams) ? $_islandora_solr_queryclass->internalSolrParams : [];

    $profiles = \Drupal::moduleHandler()->invokeAll("islandora_solr_primary_display");
    // Get the table settings.
    $config = \Drupal::config('islandora_solr.settings');
    $cache_meta->addCacheableDependency($config);
    $primary_display_array = $config->get('islandora_solr_primary_display_table');
    if (!empty($primary_display_array)) {

      $weight = $primary_display_array['weight'];
      $enabled = $primary_display_array['enabled'];
      $default = $primary_display_array['default'];
      // Sort by weight.
      array_multisort(array_values($weight), SORT_ASC, array_keys($weight), SORT_ASC, $weight);

      $list_items = [];

      foreach ($weight as $key => $value) {
        if ($enabled[$key] !== 0) {
          if (isset($params['display'])) {
            $current_display = $params['display'];
          }
          else {
            $current_display = $default;
          }

          $display_name = $profiles[$key]['name'];

          $display_description = isset($profiles[$key]['description']) ? $profiles[$key]['description'] : NULL;

          $query = array_merge($params, ['display' => $key]);

          $attr = new Attribute();
          $class = [];
          $class[] = 'display-' . Unicode::strtolower(preg_replace('/[^A-Za-z0-9]/', '-', $key));
          if ($key == $current_display) {
            $class[] = 'active';
          }
          $attr['class'] = $class;
          $attr['rel'] = 'nofollow';
          $attr['title'] = $display_description;
          $attr['href'] = Url::fromRoute('<current>', [], ['query' => $query])->toString();

          // XXX: We're not using l() because of active classes.
          // @link http://drupal.org/node/41595 @endlink
          $item['#markup'] = '<a' . $attr . '>' . Html::escape($display_name) . '</a>';

          // Add link to list.
          $list_items[] = $item;
        }
      }
      // Theme item list (if more than one displays are available).
      if (count($list_items) > 1) {
        $output = [
          '#theme' => 'item_list',
          '#items' => $list_items,
          '#title' => NULL,
          '#type' => 'ul',
          '#attributes' => ['class' => ['islandora-solr-display']],
        ];
      }
    }

  }

  $cache_meta->applyTo($output);

  return $output;
}

/**
 * Provides a list to change the sorting on a current search query.
 *
 * @global IslandoraSolrQueryProcessor $_islandora_solr_queryclass
 *   The IslandoraSolrQueryProcessor object which includes the current query
 *   settings and the raw Solr results.
 *
 * @return string
 *   A rendered list containing Solr sort links.
 */
function islandora_solr_sort() {
  $function = '_' . __FUNCTION__;
  $elements = $function();
  return \Drupal::service('renderer')->render($elements);
}

/**
 * Provides a list to change the sorting on a current search query.
 *
 * @global IslandoraSolrQueryProcessor $_islandora_solr_queryclass
 *   The IslandoraSolrQueryProcessor object which includes the current query
 *   settings and the raw Solr results.
 *
 * @return arrau
 *   A renderable array containing Solr sort links.
 */
function _islandora_solr_sort() {
  module_load_include('inc', 'islandora', 'includes/utilities');
  global $_islandora_solr_queryclass;

  $output = [];

  $cache_meta = (new CacheableMetadata())
    ->addCacheContexts([
      'url',
    ]);

  // Check if the page is dealing with search results.
  if (islandora_solr_results_page($_islandora_solr_queryclass)) {
    $cache_meta->addCacheableDependency($_islandora_solr_queryclass);

    // Parameters set in URL.
    $params = isset($_islandora_solr_queryclass->internalSolrParams) ? $_islandora_solr_queryclass->internalSolrParams : [];

    $sort_terms = islandora_solr_get_fields('sort_fields');

    // Get current sort (check in the actual parameters used for the query,
    // because modules might have altered it).
    if (isset($_islandora_solr_queryclass->solrParams['sort'])) {
      $sort_explode = preg_split(
        ISLANDORA_SOLR_QUERY_SPLIT_REGEX,
        $_islandora_solr_queryclass->solrParams['sort']
      );
      $current_sort_term = stripslashes($sort_explode[0]);
      $current_sort_order = isset($sort_explode[1]) ? $sort_explode[1] : NULL;
    }
    else {
      // If no parameters are given, internally solr defaults to 'score desc'
      // http://wiki.apache.org/solr/CommonQueryParameters#sort
      $current_sort_term = 'score';
      $current_sort_order = 'desc';
    }

    $list_items = [];

    foreach ($sort_terms as $term => $label) {
      $indicator = '';

      // Create query and indicator arrow.
      if ($term == $current_sort_term) {
        if (isset($current_sort_order) && $current_sort_order == 'asc') {
          $order = 'desc';
          $indicator = t('(DESC)');
        }
        else {
          $order = 'asc';
          $indicator = t('(ASC)');
        }
      }
      // The term is score or a date, we want descending order by default.
      elseif ($term == 'score' || stripos($term, 'date')) {
        $order = 'desc';
      }
      else {
        $order = 'asc';
      }
      $sort_param = $term . ' ' . $order;

      $query = array_merge($params, ['sort' => $sort_param]);

      $attributes = [
        'sort' => [
          'attr' => [
            'class' => [],
          ],
          'query' => $query,
          'path' => Url::fromRoute("<current>")->toString(),
        ],
      ];

      $attr = new Attribute();
      if ($term == $current_sort_term) {
        $attr['class'] = ['active'];
      }
      $attr['rel'] = 'nofollow';
      $attr['title'] = $label;
      $attr['href'] = Url::fromRoute('<current>', [], ['query' => $query])->toString();
      $attributes['sort']['attr'] = $attr;

      $hooks = islandora_build_hook_list(ISLANDORA_SOLR_FACET_BUCKET_CLASSES_HOOK_BASE);
      \Drupal::moduleHandler()->alter($hooks, $attributes, $_islandora_solr_queryclass);
      // XXX: We're not using l() because of active classes.
      // @link http://drupal.org/node/41595 @endlink
      $item['#markup'] = '<a' . $attr . '>' . $label . ' ' . $indicator . '</a>';

      // Add link to list.
      $list_items[] = $item;
    }

    // Theme item list.
    if (!empty($list_items)) {
      $output += [
        '#theme' => 'item_list',
        '#items' => $list_items,
        '#title' => NULL,
        '#type' => 'ul',
        '#attributes' => ['class' => ['islandora-solr-sort']],
      ];
    }
  }

  $cache_meta->applyTo($output);
  return $output;
}

/**
 * Search results navigation block.
 *
 * Pull params out of the URL to determine search id
 * and object offset.
 *
 * We use offset because the pid of a search result can be altered.
 * eg: Book pages
 *
 * @return string
 *   The rendered html for the block content.
 */
function islandora_solr_search_navigation() {
  $function = '_' . __FUNCTION__;
  $elements = $function();
  return \Drupal::service('renderer')->render($elements);
}

/**
 * Search results navigation block.
 *
 * Pull params out of the URL to determine search id
 * and object offset.
 *
 * We use offset because the pid of a search result can be altered.
 * eg: Book pages
 *
 * @return array
 *   The rendererable array for the block content.
 */
function _islandora_solr_search_navigation() {
  $return = [];

  $params = \Drupal::request()->query->all();
  $config = \Drupal::config('islandora_solr.settings');
  $cache_meta = (new CacheableMetadata())
    ->addCacheableDependency($config)
    ->addCacheContexts([
      'url',
      'session',
    ]);
  if ($config->get('islandora_solr_search_navigation') &&
    isset($params['solr_nav']['id']) &&
    isset($params['solr_nav']['page']) &&
    isset($params['solr_nav']['offset']) &&
    isset($_SESSION['islandora_solr_search_nav_params'][$params['solr_nav']['id']])) {
    $nav_params = $_SESSION['islandora_solr_search_nav_params'][$params['solr_nav']['id']];

    $prev_solr_object = NULL;
    $prev_offset = NULL;
    $prev_page = NULL;
    $next_solr_object = NULL;
    $next_offset = NULL;
    $next_page = NULL;
    $offset = $params['solr_nav']['offset'];
    $page = $params['solr_nav']['page'];
    $query_processor = new IslandoraSolrQueryProcessor();
    $query_processor->solrQuery = $nav_params['query'];
    $query_processor->internalSolrQuery = $nav_params['query_internal'];
    $query_processor->solrLimit = $nav_params['limit'];
    $query_processor->internalSolrParams = $nav_params['params_internal'];
    $query_processor->solrParams = $nav_params['params'];
    $limit = $nav_params['limit'];
    $query_processor->solrStart = $page * $limit;

    // Get solr objects for current page.
    $query_processor->executeQuery();

    $cache_meta->addCacheableDependency($query_processor);

    $objects = [];
    if (!empty($query_processor->islandoraSolrResult)) {
      $objects = $query_processor->islandoraSolrResult['response']['objects'];
    }

    // Try get previous object on the current page.
    if ($offset != 0) {
      if (isset($objects[$offset - 1])) {
        $prev_solr_object = $objects[$offset - 1];
        $prev_offset = $offset - 1;
        $prev_page = $page;
      }
    }

    // If previous object can't be found on current page,
    // go to the previous page and select the last object.
    if (!$prev_solr_object && $page != 0) {
      $query_processor->solrStart = ($page - 1) * $limit;
      $query_processor->executeQuery();
      if (!empty($query_processor->islandoraSolrResult) && !empty($query_processor->islandoraSolrResult['response']['objects'])) {
        $prev_objects = $query_processor->islandoraSolrResult['response']['objects'];
        end($prev_objects);
        $prev_offset = key($prev_objects);
        $prev_page = $page - 1;
        $prev_solr_object = $prev_objects[$prev_offset];
      }
    }

    // Try get next object on the current page.
    if (isset($objects[$offset + 1])) {
      $next_solr_object = $objects[$offset + 1];
      $next_offset = $offset + 1;
      $next_page = $page;
    }

    // If next object can't be found on current page,
    // go to the next page and select the first object.
    if (!$next_solr_object) {
      $limit = $query_processor->solrLimit;
      $query_processor->solrStart = ($page + 1) * $limit;
      $query_processor->executeQuery();
      if (!empty($query_processor->islandoraSolrResult) && !empty($query_processor->islandoraSolrResult['response']['objects'])) {
        $next_objects = $query_processor->islandoraSolrResult['response']['objects'];
        $next_offset = 0;
        $next_page = $page + 1;
        $next_solr_object = $next_objects[$next_offset];
      }
    }

    $return_link = NULL;
    $prev_link = NULL;
    $next_link = NULL;
    // Get URL details from the prev and next objects
    // to populate navigation links.
    if (isset($nav_params['path'])) {
      $return_query = array_merge($query_processor->internalSolrParams, ['page' => $page]);
      $return_link = Url::fromUri(
        $nav_params['path'],
        ['query' => $return_query, 'absolute' => TRUE]
      )->toString();
    }
    if ($prev_solr_object) {
      $prev_params = $prev_solr_object['object_url_params'];
      $prev_params['solr_nav']['offset'] = $prev_offset;
      $prev_params['solr_nav']['page'] = $prev_page;
      $prev_params['solr_nav']['id'] = $params['solr_nav']['id'];
      $prev_link = Url::fromUri(
        $prev_solr_object['object_url'],
        [
          'query' => $prev_params,
          'fragment' => (isset($prev_solr_object['object_url_fragment']) ? $prev_solr_object['object_url_fragment'] : ''),
          'absolute' => TRUE,
        ]
      )->toString();
    }

    if ($next_solr_object) {
      $next_params = $next_solr_object['object_url_params'];
      $next_params['solr_nav']['offset'] = $next_offset;
      $next_params['solr_nav']['page'] = $next_page;
      $next_params['solr_nav']['id'] = $params['solr_nav']['id'];
      $next_link = Url::fromUri(
        $next_solr_object['object_url'],
        [
          'query' => $next_params,
          'fragment' => (isset($next_solr_object['object_url_fragment']) ? $next_solr_object['object_url_fragment'] : ''),
          'absolute' => TRUE,
        ]
      )->toString();
    }
    $return = [
      '#theme' => 'islandora_solr_search_navigation_block',
      '#return_link' => $return_link,
      '#return_text' => t('Return to search'),
      '#prev_link' => $prev_link,
      '#prev_text' => t('Previous Object'),
      '#next_link' => $next_link,
      '#next_text' => t('Next Object'),
    ];
  }

  $cache_meta->applyTo($return);
  return $return;
}

/**
 * Search results page size block.
 *
 * Use the limit URL param to increase or decrease the result page size.
 *
 * @return string
 *   The rendered html for the block content.
 */
function islandora_solr_search_results_limit() {
  $function = '_' . __FUNCTION__;
  $elements = $function();
  return \Drupal::service('renderer')->render($elements);
}

/**
 * Search results page size block.
 *
 * Use the limit URL param to increase or decrease the result page size.
 *
 * @return array
 *   The renderable array for the block content.
 */
function _islandora_solr_search_results_limit() {
  global $_islandora_solr_queryclass;

  $cache_meta = (new CacheableMetadata())
    ->addCacheContexts([
      'url',
    ]);

  // Check if the page is dealing with search results.
  if (islandora_solr_results_page($_islandora_solr_queryclass)) {
    $config = \Drupal::config('islandora_solr.settings');

    $cache_meta
      ->addCacheableDependency($_islandora_solr_queryclass)
      ->addCacheableDependency($config);
    $default_limit = $config->get('islandora_solr_num_of_results');
    $limits = $config->get('islandora_solr_num_of_results_advanced');
    $params = \Drupal::request()->query->all();
    if (isset($params['limit']) && is_numeric($params['limit'])) {
      $default_limit = $params['limit'];
    }
    $limit_list = [];
    foreach ($limits as $limit) {
      if ($limit == $default_limit) {
        $limit_list[] = $limit;
      }
      else {
        $attr = new Attribute();
        $attr['rel'] = 'nofollow';
        $query = $params;
        $query['limit'] = $limit;
        unset($query['page']);

        $attr['href'] = Url::fromRoute('<current>', [], ['query' => $query])->toString();

        // XXX: We're not using l() because of active classes.
        // @link http://drupal.org/node/41595 @endlink
        $limit_list[]['#markup'] = '<a' . $attr . '>' . $limit . '</a>';
      }
    }

    if (!empty($limit_list)) {
      $list = [
        '#theme' => 'item_list',
        '#items' => $limit_list,
        '#title' => NULL,
        '#type' => 'ul',
        '#attributes' => ['id' => 'islandora_solr_results_limit'],
      ];
      $cache_meta->applyTo($list);
      return $list;
    }
  }
}
