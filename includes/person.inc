<?php

/**
 * @file
 * Handles creation of a users Islandore data collection and Entity object.
 */

/**
 * Verify that the users entity and collection object exist in islandora/fedora.
 *
 * @todo Give the user the option to recreate their entity or collection object.
 * Don't force it just in case the failure is temporary.
 * @param $account
 *   The users drupal account object
 */
function islandora_pydio_bridge_verify_user_objects($account) {
  $collection_pid = $account->data['islandora_pydio_bridge_person_data_collection'];
  $person_collection_object = islandora_object_load($collection_pid);
  if (FALSE === $person_collection_object) {
    drupal_set_message(t("It looks like your Data collection was deleted, We will recreate it but some data may be lost!"));
    islandora_pydio_bridge_create_user_collection($account, TRUE);
  }
  $person_pid = $account->data['islandora_pydio_bridge_person_entity'];
  $person_entity_object = islandora_object_load($person_pid);
  if (FALSE === $person_entity_object) {
    drupal_set_message(t("It looks like your Personal Entity object was deleted, We will recreate it but some data may be lost!"));
    islandora_pydio_bridge_create_user_entity($account, TRUE);
  }
}

/**
 * Creates a collection object in fedora and stores the pid in the drupal users account.
 *
 * @param $account
 *   The drupal users account object
 * @param bool|FALSE $force
 *   If true force the creation of the object even if there is a pid stored in the drupal
 *   users account.
 */
function islandora_pydio_bridge_create_user_collection($account, $force = FALSE) {
  if (empty($account->data['islandora_pydio_bridge_person_data_collection']) || $force == TRUE) {
    try {
      $person_collection_object = islandora_pydio_bridge_create_user_subcollection($account);
    } catch (Exception $e) {
      watchdog("islandora_pydio_bridge", "Failed to create an Islandora
    Data Collection for %user, %message", array(
        "%user" => $account->name,
        "%message" => $e->getMessage()
          ), WATCHDOG_ERROR, NULL);
    }

    if (!empty($person_collection_object)) {
      $person_collection_data = array('data' => array('islandora_pydio_bridge_person_data_collection' => $person_collection_object->id));
      user_save($account, $person_collection_data);
      watchdog("islandora_pydio_bridge", "Successfully created an Islandora
    Data Collection for %user with Islandora id of %pid", array(
        "%user" => $account->name,
        "%pid" => $person_collection_object->id
          ), WATCHDOG_INFO, NULL);
    }
  }
}

/**
 * Creates the users Islandora Entity object and stores it's pid in the drupal users acount.
 *
 * @param $account
 *   The drupal users account object
 * @param bool|FALSE $force
 *   If true force the creation of the object even if there is a pid stored in the drupal
 *   users account.
 */
function islandora_pydio_bridge_create_user_entity($account, $force = FALSE) {
  if (empty($account->data['islandora_pydio_bridge_person_entity']) ||
      $force == TRUE) {
    try {
      $person_object = islandora_pydio_bridge_create_person($account);
    } catch (Exception $e) {
      watchdog("islandora_pydio_bridge", "Failed to create an Islandora Person
    object for %user, %message", array(
        "%user" => $account->name,
        "%message" => $e->getMessage()
          ), WATCHDOG_ERROR, NULL);
    }
    if (!empty($person_object)) {
      $person_data = array('data' => array('islandora_pydio_bridge_person_entity' => $person_object->id));
      user_save($account, $person_data);
      watchdog("islandora_pydio_bridge", "Successfully created an Islandora Person
    object for %user with Islandora id of %pid", array(
        "%user" => $account->name,
        "%pid" => $person_object->id
          ), WATCHDOG_INFO, NULL);
    }
  }
}

/**
 * Creates a basic object ready for ingest or adding additional datastreams.
 *
 * Since we are calling some functions from the user_login hook we may not have
 * a fully bootstrapped user so we use the info from the account object.
 *
 * @param object $account
 *   A drupal account object.
 * @param array $content_models
 *   An array of content models to which this object will subscribe.
 * @param array $relationships
 *   An array of relationships we want to add to this object.
 *
 * @return \AbstractObject
 *   An object ready for ingest.
 */
function islandora_pydio_bridge_create_basic_object($account, $content_models, $relationships, $label) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  $url = variable_get('islandora_base_url', 'http://localhost:8080/fedora');
  // user may not be bootstrapped yet so get the info from the account, can't use islandora_prepare_object as we may still be anonymous
  $username = $account->name;
  $password = $account->pass;
  $connection = new RepositoryConnection($url, $username, $password);
  $repository = new FedoraRepository(
      new FedoraApi($connection), new SimpleCache()
  );
  $object = $repository->constructObject(variable_get('islandora_pydio_bridge_pid_namespace', 'pydio'));
  $object->owner = $account->name;
  $object->label = $label;
  foreach ($content_models as $content_model) {
    $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', $content_model);
  }

  foreach ($relationships as $relationship) {
    $object->relationships->add(FEDORA_RELS_EXT_URI, $relationship['relationship'], $relationship['pid']);
  }
  return $object;
}

/**
 * Creates a users data subcollection under the Pydio Data collection.
 *
 * @param object $account
 *   A Drupal account object
 *
 * @return FedoraObject
 *   An islandora FedoraObject
 */
function islandora_pydio_bridge_create_user_subcollection($account) {
  $content_models = array("islandora:collectionCModel");
  $parent_collection = variable_get("islandora_pydio_bridge_repository_pid", "islandora:pydio_data_top");
  $relationships = array(array(
      'relationship' => 'isMemberOfCollection',
      'pid' => $parent_collection
  ));
  $label = $account->name . " Data Collection";
  $object = islandora_pydio_bridge_create_basic_object($account, $content_models, $relationships, $label);

  $xacml = new IslandoraXacml($object);
  $xacml->viewingRule->addUser(array($object->owner, 'admin'));
  $xacml->managementRule->addUser(array($object->owner, 'admin'));
  $xacml->writeBackToFedora();
  $mods_ds = $object->constructDatastream('MODS', 'M');
  $mods_ds->label = 'MODS Record';
  $mods_ds->mimeType = 'application/xml';
  $ds_text = islandora_pydio_bridge_mods_template();
  $ds_text = str_ireplace("MODSTITLE", $label, $ds_text);
  $ds_text = str_ireplace("USERID", $account->name, $ds_text);
  $mods_ds->setContentFromString($ds_text);
  $object->ingestDatastream($mods_ds);
  $collection_policy = islandora_pydio_bridge_get_collection_policy();
  if (!empty($collection_policy)) {
    $cp_ds = $object->constructDatastream('COLLECTION_POLICY', 'M');
    $cp_ds->label = 'COLLECTION POLICY';
    $cp_ds->setContentFromString($collection_policy);
    $object->ingestDatastream($cp_ds);
  }
  else {
    watchdog("islandora_pydio_bridge", "Failed to add a Collection Policy
     to %user Data Collection Object, %message", array(
      "%user" => $account->name,
      "%message" => "Could not find a suitable Collection Policy"
        ), WATCHDOG_ERROR, NULL);
  }
  return islandora_add_object($object);
}

/**
 * Returns an Islandora Collection policy xml with an updated namespace.
 *
 * @return mixed|null|string
 *   null if it can't load the default collection  policy or the collection
 * policy string.
 */
function islandora_pydio_bridge_get_collection_policy() {
  $path = drupal_get_path('module', 'islandora_pydio_bridge');
  $file = $path . '/xml/collection_policy.xml';
  $collection_policy = file_get_contents($file);
  $namespace = variable_get('islandora_pydio_bridge_pid_namespace', 'pydio');
  if (!empty($collection_policy)) {
    $collection_policy = str_replace('FAKENAMESPACE', $namespace, $collection_policy);
    return $collection_policy;
  }
  return NULL;
}

/**
 * Create a personCModel entity object.
 *
 * @param object $account
 *   A Drupal account object
 *
 * @return FedoraObject
 *   An Islandora FedoraObject
 *
 */
function islandora_pydio_bridge_create_person($account) {
  $content_models = array("islandora:entityCModel", "islandora:personCModel");
  $parent_collection = variable_get("islandora_entities_people_collection", "islandora:entity_collection");
  $relationships = array(array(
      'relationship' => 'isMemberOfCollection',
      'pid' => $parent_collection
  ));

  $object = islandora_pydio_bridge_create_basic_object($account, $content_models, $relationships, $account->name);

  $xacml = new IslandoraXacml($object);
  $xacml->viewingRule->addUser(array($object->owner, 'admin'));
  $xacml->managementRule->addUser(array($object->owner, 'admin'));
  $xacml->writeBackToFedora();
  $mads_ds = $object->constructDatastream('MADS', 'M');
  $mads_ds->label = 'MADS Record';
  $mads_ds->mimeType = 'application/xml';
  $ds_text = islandora_pydio_bridge_mads_template();
  $ds_text = str_ireplace("USEREMAIL", $account->mail, $ds_text);
  $ds_text = str_ireplace("USERID", $account->name, $ds_text);
  $mads_ds->setContentFromString($ds_text);
  $object->ingestDatastream($mads_ds);
  return islandora_add_object($object);
}

/**
 * Returns a MADS string template.
 *
 * @return string
 *   A mads xml template
 */
function islandora_pydio_bridge_mads_template() {
  return <<<TAG
<mads xmlns="http://www.loc.gov/mads/v2" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:mads="http://www.loc.gov/mads/v2">
  <authority><name type="personal"><namePart type="given"></namePart><namePart type="family"></namePart><namePart type="date"></namePart></name><titleInfo><title></title></titleInfo></authority>
  <variant><name><namePart type="given"/><namePart type="family"/></name></variant>
  <affiliation><organization></organization><organization></organization><position></position><email>USEREMAIL</email><phone></phone><dateValid point="start"/><dateValid point="end"/></affiliation>
  <fieldOfActivity></fieldOfActivity>
  <identifier type="u1">USERID</identifier>
  <note type="status"></note>
  <note type="history"></note>
  <note></note>
  <note type="address"/>
  <url></url>
</mads>
TAG;
}

/**
 * Returns a MODS string template.
 *
 * @return string
 *   A mods xml template
 */
function islandora_pydio_bridge_mods_template() {
  return <<<TAG
<mods xmlns="http://www.loc.gov/mods/v3" xmlns:mods="http://www.loc.gov/mods/v3" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <titleInfo>
    <title>MODSTITLE</title>
    <subTitle></subTitle>
  </titleInfo>
  <identifier type="u1">USERID</identifier>
  <subject>
    <topic>Research Data</topic>
    <hierarchicalGeographic>
      <continent></continent>
    <country></country><province></province><region></region><county></county><city></city><citySection></citySection></hierarchicalGeographic>
  <geographic></geographic><temporal></temporal><cartographics><coordinates></coordinates></cartographics></subject>
  <originInfo>
    <dateCaptured></dateCaptured>
    <dateIssued></dateIssued>
  <publisher></publisher><place><placeTerm authority="marccountry" type="code"></placeTerm></place><place><placeTerm type="text"></placeTerm></place></originInfo>
  <physicalDescription>
    <extent></extent>
    <note></note>
    <form authority="marcform"></form>
  </physicalDescription>
  <abstract></abstract>
  <accessCondition type="use and reproduction"></accessCondition>
  <location>
    <physicalLocation displayLabel="Department"></physicalLocation>
    <shelfLocator></shelfLocator>
  </location>
  <name type="personal">
    <namePart></namePart>
  <role><roleTerm authority="marcrelator" type="text"></roleTerm></role></name>
  <name type="corporate">
    <namePart></namePart>
  <role><roleTerm authority="marcrelator" type="text"></roleTerm></role></name>

  <typeOfResource></typeOfResource>
<genre authority="lctgm"></genre><language><languageTerm authority="iso639-2b" type="code"></languageTerm></language><identifier type="local"></identifier><note></note></mods>
TAG;
}
