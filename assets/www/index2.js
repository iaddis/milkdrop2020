'use strict';

(function() {


  function importTrace($timelineView, url, trace) {

    console.log('importTrace', url);

    const m = new tr.Model();

    // const $trackDetailedModelStats = tr.ui.b.findDeepElementMatching(
    //     document.body, '#track-detailed-model-stats');
    const importOptions = new tr.importer.ImportOptions();
//    importOptions.trackDetailedModelStats = $trackDetailedModelStats.checked;
    const i = new tr.importer.Import(m, importOptions);
    const p = i.importTracesWithProgressDialog( [trace] );

    p.then(
        function() {

          console.log('importTrace success');

          $timelineView.model = m;
          $timelineView.updateDocumentFavicon();
          $timelineView.globalMode = true;
          $timelineView.viewTitle = url;
        },
        function(err) {

          console.log('importTrace error', err);
          const overlay = new tr.ui.b.Overlay();
          overlay.textContent =   err.toString();  //tr.b.normalizeException(err).message;
          overlay.title = 'Import error';
          overlay.visible = true;
        });
  }

  function loadTrace($timelineView, url) {
    console.log('loadTrace', url);

    $.ajax({
      url: url,
      dataType: "text"
    })
    .done(function( data ) {
      importTrace($timelineView, url, data);
    })
    .fail(function(xhr, status, err) {
      console.log('loadTrace error', status, err);
      const overlay = new tr.ui.b.Overlay();
      overlay.textContent =  err.toString();  //tr.b.normalizeException(err).message;
      overlay.title = 'Load error';
      overlay.visible = true;
    });


  }



/*
  function cleanFilename(file) {
    const m = /\/tracing\/test_data\/(.+)/.exec(file);
    const rest = m[1];

    function upcase(letter) {
      return ' ' + letter.toUpperCase();
    }

    return rest.replace(/_/g, ' ')
        .replace(/\.[^\.]*$/, '')
        .replace(/ ([a-z])/g, upcase)
        .replace(/^[a-z]/, upcase);
  }
  */

  

 $( document ).ready(function() {
    console.log( "ready!" );
                     
                     
                     let $presets = $("<div>");
                     $("body").append($presets);

                     $.get( "/api/presets", function( json ) {
                           const presets = json.presets;
                           for (let i = 0; i < presets.length; ++i) {
                                const preset = presets[i];
                                $presets.append(preset)
                               }
                       });
                     

//
//    const $timelineView = document.querySelector('tr-ui-timeline-view');
//    $timelineView.globalMode = true;
//
//
//    const $select = document.createElement('select');
//    $timelineView.leftControls.appendChild($select);
//
//
//
//    function onSelectionChange() {
//      window.location.hash = '#' + $select[$select.selectedIndex].value;
//    }
//
//
//    function onHashChange() {
//      const file = window.location.hash.substr(1);
//      if ($select.selectedIndex >= 0 && $select[$select.selectedIndex].value !== file) {
//        for (let i = 0; i < $select.children.length; i++) {
//          if ($select.children[i].value === file) {
//            $select.selectedIndex = i;
//            break;
//          }
//        }
//      }
//
//      loadTrace($timelineView, file);
//    }
//
//    window.addEventListener('hashchange', onHashChange);
//
//
//
//     $.get( "/trace_data/__file_list__", function( files ) {
//
//      for (let i = 0; i < files.length; ++i) {
//
//        let filename = "/trace_data/" + files[i];
//        if (!filename.endsWith(".trace.json"))
//          continue;
//
//        const opt = document.createElement('option');
//        opt.value = filename;
//        opt.textContent = filename; //cleanFilename(files[i]);
//        $select.appendChild(opt);
//      }
//      $select.selectedIndex = 0;
//      $select.onchange = onSelectionChange;
//
//      if (!window.location.hash) {
//        // This will trigger an onHashChange so no need to reload directly.
//        window.location.hash = '#' + $select[$select.selectedIndex].value;
//      } else {
//        onHashChange();
//      }
//    });

/*
    const $trackDetailedModelStats = tr.ui.b.createCheckBox(
        this, 'trackDetailedModelStats',
        'traceViewer.trackDetailedModelStats', false,
        'Detailed file size stats',
        onHashChange);
    $trackDetailedModelStats.id = 'track-detailed-model-stats';
    $timelineView.leftControls.appendChild($trackDetailedModelStats);
    */
  

});


  // window.addEventListener('load', onLoad);
//  window.addEventListener('HTMLImportsLoaded', onLoad);



}());
