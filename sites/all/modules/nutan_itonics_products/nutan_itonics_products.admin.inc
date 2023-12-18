<?php

function nutan_itonics_products_admin($form, &$form_state)
{
  $form['search'] = getSearchForm($form_state);
  $form['create_button'] = getCreateButton();
  $search = $form_state['input']['search'] ?? '';
  $header = getHeaders();
  $query = db_select('nutan_itonics_products', 'p')
    ->fields('p')
    ->extend('TableSort')
    ->orderByHeader($header)
    ->extend('TableSort');

  if (!empty($search)) {
    $query->condition(db_or()
      ->condition('title', '%' . db_like($search) . '%', 'LIKE')
      ->condition('category', '%' . db_like($search) . '%', 'LIKE')
      ->condition('type', '%' . db_like($search) . '%', 'LIKE')
      ->condition('owner_email', '%' . db_like($search) . '%', 'LIKE')
    );
  }

  $result = $query->extend('TableSort')
    ->orderByHeader($header)
    ->range(0, 50)
    ->execute();

  $rows = [];
  foreach ($result as $row) {
    $formatted_category = getFormattedCategory($row->category);
    $image_path = $row->image ? file_create_url(getFilePath() . $row->image) : '';
    $rows[] = getRows($row, $image_path, $formatted_category);
  }
  $form['product_table'] = [
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No products found'),
  ];
  $form['pager'] = ['#markup' => theme('pager'),];
  return $form;
}

function trimTextAfterWords($text, $numWords = 100): string
{
  $words = str_word_count($text, 1);
  $trimmedText = implode(' ', array_slice($words, 0, $numWords));
  return count($words) > $numWords ? $trimmedText . '...' : $trimmedText;
}

function getRows($row, $image_path, $formatted_category): array
{
  return
    [
      'id' => $row->id,
      'title' => trimTextAfterWords($row->title),
      'image' => getImageFormatted($image_path, $row),
      'category' => $formatted_category,
      'type' => $row->type,
      'owner_email' => $row->owner_email,
      'expiry_date' => $row->expiry_date,
      'operations' => getActionButtons($row)
    ];

}

function getActionButtons($row): string
{
  return l(t('View'), 'admin/products/' . $row->id) . ' | ' . l(t('Edit'), 'admin/products/' . $row->id . '/edit') . ' | ' . l(t('Delete'), 'admin/products/' . $row->id . '/delete', ['query' => drupal_get_destination()]);

}

function getImageFormatted(string $image_path, $row)
{
  return $image_path ? theme('image', [
    'path' => $image_path,
    'alt' => $row->title,
    'attributes' => [
      'style' => 'width: 200px; height:200px;'
    ]
  ]) : 'N/A';
}

function getFormattedCategory($category): string
{
  return !empty($category) ? implode(', ', unserialize($category)) : 'N/A';
}

function getHeaders(): array
{
  return
    [
      'id' => t('ID'),
      'title' => t('Title'),
      'image' => t('Image'),
      'category' => t('Category'),
      'type' => t('Type'),
      'owner_email' => t('Owner Email'),
      'expiry_date' => t('Expiry Date'),
      'operations' => t('Operations')];

}

function getCreateButton(): array
{
  return [
    '#markup' => l(t('Create'), 'admin/products/add'),
    '#prefix' => '<div style="display:inline-block; margin-left: 10px;">',
    '#suffix' => '</div>'
  ];
}

function getSearchForm($form_state): array
{
  return [
    '#type' => 'textfield',
    '#title' => t('Search'),
    '#size' => 60,
    '#maxlength' => 128,
    '#default_value' => $form_state['input']['search'] ?? ''
  ];
}


/**
 * Form constructor for the product details page.
 */
function nutan_itonics_products_detail($form, &$form_state, $product_id)
{
  $form['#markup'] = t('This is the product details page for product with ID @product_id.', ['@product_id' => $product_id]);
  return $form;
}

/**
 * Form constructor for the product add page.
 */
function nutan_itonics_products_add($form, &$form_state): array
{
  $form = getCommonForm($form, [], []);
  $form['submit'] = array('#type' => 'submit', '#value' => t('Save'),);
  return $form;
}

/**
 * Form submit handler for the product add form.
 */
function nutan_itonics_products_add_submit($form, &$form_state): void
{
  $category = getSerializedCategory($form_state);
  $file = file_save_upload('image', array('file_validate_is_image' => [], 'file_validate_extensions' => ['png gif jpg jpeg']));
  $unique_filename = '';
  if ($file) {
    $unique_filename = 'itonics_' . uniqid() . '.' . pathinfo($file->filename, PATHINFO_EXTENSION);
    if (!file_move($file, getFilePath() . $unique_filename, FILE_EXISTS_REPLACE))
      form_set_error('image', t('Failed to move the uploaded file.'));
  }
  $values = $form_state['values'];
  storeProduct($values, $category, $unique_filename);
  drupal_set_message(t('Product saved successfully.'));
  $form_state['redirect'] = 'admin/products';
}

function getFilePath(): string
{
  return 'public://products/';
}

function storeProduct(array $values, string $category, string $unique_filename): void
{
  db_insert('nutan_itonics_products')
    ->fields([
      'title' => $values['title'],
      'image' => $unique_filename,
      'summary' => $values['summary'],
      'description' => $values['description']['value'],
      'category' => $category,
      'type' => $values['type'],
      'owner_email' => $values['owner_email'],
      'expiry_date' => ($values['expiry_date'] ?? date('Y-m-d')),
    ])->execute();
}

/**
 * Validation handler for the owner_email field.
 */
function nutan_itonics_products_email_validate($element, &$form_state, $form): void
{
  $value = $form_state['values']['owner_email']; // Use the correct form element name
  if (!filter_var($value, FILTER_VALIDATE_EMAIL)) {
    form_error($element, t('The email address is not valid.'));
  }
}

function nutan_itonics_products_edit_form($form, &$form_state, $product_id): array
{
  $product = getProductById($product_id);
  $category = unserialize($product['category']);
  $form = getCommonForm($form, $category, $product);
  $form['submit'] = array('#type' => 'submit', '#value' => t('Save Changes'),);
  $form['product_id'] = array('#type' => 'value', '#value' => $product_id,);
  return $form;
}

function getCommonForm($form, array $category, $product = null): array
{
  $form['title'] = [
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#required' => TRUE,
    '#default_value' => $product['title'] ?? '',
  ];
  $form['image'] = [
    '#type' => 'managed_file',
    '#title' => t('Image'),
    '#description' => t('Upload an image for the product.'),
    '#upload_location' => getFilePath(),
//    '#required' => !$product,
    '#upload_validators' => ['file_validate_extensions' => ['png gif jpg jpeg'],
    ],
  ];
  $form['summary'] = [
    '#type' => 'textarea',
    '#title' => t('Summary'),
    '#default_value' => $product['summary'] ?? '',
  ];
  $form['description'] = [
    '#type' => 'text_format',
    '#title' => t('Description'),
    '#format' => 'filtered_html',
    '#default_value' => $product['description'] ?? ''
  ];
  $form['category'] = [
    '#type' => 'select',
    '#title' => t('Category'),
    '#options' => [
      'option1' => t('Option 1'),
      'option2' => t('Option 2'),
      'option3' => t('Option 3'),
      'option4' => t('Option 4'),
    ],
    '#required' => TRUE,
    '#attributes' => ['class' => ['jquery-ui-multiselect'],],
    '#attached' => [
      'js' => [
        drupal_get_path('module',
          'jquery_ui_multiselect') . '/jquery.ui.multiselect.js',], 'library' => ['jquery_ui_multiselect/jquery.ui.multiselect'
      ]
    ],
    '#multiple' => TRUE,
    '#default_value' => $category,
  ];
  $form['type'] = [
    '#type' => 'radios',
    '#title' => t('Type'),
    '#options' => [
      'type1' => t('Type 1'),
      'type2' => t('Type 2'),
    ],
    '#required' => TRUE,
    '#default_value' => $product['type'] ?? '',
  ];
  $form['owner_email'] = [
    '#type' => 'textfield',
    '#title' => t('Owner Email'),
    '#required' => TRUE,
    '#element_validate' => ['nutan_itonics_products_email_validate'],
    '#default_value' => $product['owner_email'] ?? '',
  ];
  $form['expiry_date'] = [
    '#type' => 'date_popup',
    '#title' => t('Expiry Date'),
    '#date_format' => 'Y-m-d',
    '#date_increment' => 1,
//    '#required' => TRUE,
    '#default_value' => $product['expiry_date'] ?? '',
  ];
  return $form;
}

function getProductById($product_id)
{
  return db_select('nutan_itonics_products', 'p')
    ->fields('p')
    ->condition('id', $product_id)
    ->execute()
    ->fetchAssoc();
}

/**
 * Form submission handler for editing a product.
 */
function nutan_itonics_products_edit_form_submit($form, &$form_state): void
{
  $category = getSerializedCategory($form_state);
  $product_id = arg(2);
  $existing_product = getProductById($product_id);
  $file = file_save_upload('image', [
    'file_validate_is_image' => [],
    'file_validate_extensions' => ['png gif jpg jpeg'],
  ]);
  if ($file && $file->fid) {
    $filename = 'itonics_' . uniqid() . '.' . pathinfo($file->filename, PATHINFO_EXTENSION);
    if (!file_move($file, getFilePath() . $filename, FILE_EXISTS_REPLACE)) {
      form_set_error('image', t('Failed to move the uploaded file.'));
      return;
    }
  } else {
    $filename = $existing_product['image'];
  }
  $values = $form_state['values'];
  updateProduct($values, $category, $filename, $product_id);
  drupal_set_message(t('Product updated successfully.'));
  $form_state['redirect'] = 'admin/products';
}

function getSerializedCategory($form_state): string
{
  return serialize($form_state['values']['category']) ?? '';
}

function updateProduct(array $values, string $category, string $unique_filename, string $product_id): void
{
  db_update('nutan_itonics_products')
    ->fields([
      'title' => $values['title'],
      'image' => $unique_filename,
      'summary' => $values['summary'],
      'description' => $values['description']['value'],
      'category' => $category,
      'type' => $values['type'],
      'owner_email' => $values['owner_email'],
      'expiry_date' => ($values['expiry_date'] ?? date('Y-m-d')),
    ])->condition('id', $product_id)
    ->execute();
}


/**
 * Form callback for deleting a product.
 */
function nutan_itonics_products_delete_form($form, &$form_state, $product_id)
{
  $product = getProductById($product_id);
  $form['title'] = ['#markup' => t('Are you sure you want to delete the product %title?', ['%title' => $product['title']]),];
  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Delete Product'),
    '#attributes' => ['onclick' => 'return confirm("' . t('Are you sure you want to delete this product?') . '");'],
  ];
  $form_state['product_id'] = $product_id;
  return $form;
}

/**
 * Form submission handler for deleting a product.
 */
function nutan_itonics_products_delete_form_submit($form, &$form_state): void
{
  $product_id = $form_state['product_id'];
  deleteProduct($product_id);
  drupal_set_message(t('Product deleted successfully.'));
  $form_state['redirect'] = 'admin/products';
}

function deleteProduct(string $product_id): void
{
  if (getProductById($product_id)['image'])
    unlink(getFilePath() . getProductById($product_id)['image']);
  db_delete('nutan_itonics_products')
    ->condition('id', $product_id)
    ->execute();
}

function nutan_itonics_products_view_form($form, &$form_state, $product_id)
{
  $product = getProductById($product_id);
  $form['title'] = ['#markup' => '<h2>' . check_plain($product['title']) . '</h2>',];
  $form['image'] = [
    '#markup' => isset($product['image']) ?
      theme('image', [
        'path' => getFilePath() . $product['image'],
        'attributes' => [
          'style' => 'width: 200px; height: 200px;', // Adjust the width as needed
        ],
      ])
      : 'No Image',
  ];
  $form['summary'] = ['#markup' => '<p>Summary: ' . check_plain($product['summary']) . '</p>',];
  $form['description'] = ['#markup' => '<p>Description: ' . check_plain($product['description']) . '</p>',];
  $form['category'] = ['#markup' => '<p>Category: ' . implode(', ', unserialize($product['category'])) . '</p>',];
  $form['type'] = ['#markup' => '<p>Type: ' . check_plain($product['type']) . '</p>',];
  $form['owner_email'] = ['#markup' => '<p>Owner Email: ' . check_plain($product['owner_email']) . '</p>',];
  $form['expiry_date'] = ['#markup' => '<p>Expiry Date: ' . check_plain($product['expiry_date']) . '</p>',];
  return $form;
}
