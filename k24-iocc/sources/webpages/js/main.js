var webserver = {
    init: function() {
        /* obtain a pointer to the rgbled0_form and its result */
        webserver.rgbled0_form = document.getElementById('rgbled0_form');
        webserver.rgbled0_status_div = document.getElementById('rgbled0_status');

        /* listen for the submit button press */
        YAHOO.util.Event.addListener(webserver.rgbled0_form, 'submit', webserver.led_submit);

        /* obtain a pointer to the rgbled1_form and its result */
        webserver.rgbled1_form = document.getElementById('rgbled1_form');
        webserver.rgbled1_status_div = document.getElementById('rgbled1_status');

        /* listen for the submit button press */
        YAHOO.util.Event.addListener(webserver.rgbled1_form, 'submit', webserver.led_submit);

        /* obtain a pointer to the switch_form and its result */
        webserver.switch_form = document.getElementById('switch_form');
        webserver.switch_status_div = document.getElementById('switch_status');

        /* listen for the submit button press */
        YAHOO.util.Event.addListener(webserver.switch_form, 'submit', webserver.switch_submit);

        /* make a XMLHTTP request */
        YAHOO.util.Connect.asyncRequest('POST', '/cmd/switchxhr', webserver.switch_callback);

        webserver.factest_button = document.getElementById('factest_button');
        YAHOO.util.Event.addListener(webserver.factest_button, 'click', webserver.factest_button_clicked);
    },

    factest_button_clicked: function(e) {

        YAHOO.util.Connect.resetFormState();

        /* make a XMLHTTP request */
        YAHOO.util.Connect.asyncRequest('Get', '/factest_results.html', webserver.factest_button_callback);
    },
    factest_button_callback: {
        success: function(o) {
          var pom = document.createElement('a');
          function strip(html){
             let doc = new DOMParser().parseFromString(html, 'text/html');
             return doc.body.textContent || "";
          }
          pom.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(strip(o.responseText)));
          pom.setAttribute('download', "factest_results.log");
          pom.click();
        },

        failure: function(o) {
            alert('LEDXHR request failed: ' + o.statusText);
        },

        timeout: 3000
    },

    led_submit: function(e) {
        YAHOO.util.Event.preventDefault(e);
        form = e.target;
        YAHOO.util.Connect.setForm(form);

        /* make a XMLHTTP request */
        YAHOO.util.Connect.asyncRequest('POST', '/cmd/ledxhr', webserver.led_callback);
    },

    led_callback: {
        success: function(o) {
            /* This turns the JSON string into a JavaScript object. */
            a = o.responseText.split('&');
            rgbled=a[0].split('=')[1]
            color=a[1].split('=')[1]

            if(rgbled != 0 && rgbled != 1) {
                alert('LEDXHR request failed: unknown LED ');
            }

            /* Modify led box */
            led_status_div = eval("webserver.rgbled"+rgbled+"_status_div")

            switch(color) {
                case "0":
                led_status_div.innerHTML='<div class="led-off"></div>';
                break;
                case "1":
                led_status_div.innerHTML='<div class="led-red"></div>';
                break;
                case "2":
                led_status_div.innerHTML='<div class="led-green"></div>';
                break;
                case "3":
                led_status_div.innerHTML='<div class="led-blue"></div>';
                break;
                default:
                led_status_div.innerHTML='<p><span style="background:#fb6767"> ERROR </span></p>';
            }
        },

        failure: function(o) {
            alert('LEDXHR request failed: ' + o.statusText);
        },

        timeout: 3000
    },

    switch_submit: function(e) {
        YAHOO.util.Event.preventDefault(e);
        YAHOO.util.Connect.setForm(webserver.switch_form);

        /* disable the form until result is obtained */
        for(var i=0; i<webserver.switch_form.elements.length; i++) {
            webserver.switch_form.elements[i].disabled = true;
        }

        /* make a XMLHTTP request */
        YAHOO.util.Connect.asyncRequest('POST', '/cmd/switchxhr', webserver.switch_callback);

        var result_fade_out = new YAHOO.util.Anim(webserver.switch_status_div, 
            {opacity: { to: 0 }},
            0.25, YAHOO.util.Easing.easeOut);
        result_fade_out.animate();
    },

    switch_callback: {
        success: function(o) {
            /* This turns the JSON string into a JavaScript object. */
            var response = eval('(' + o.responseText + ')');

            // Set up the animation on the results div.
            var result_fade_in = new YAHOO.util.Anim(webserver.switch_status_div, {
                opacity: { to: 1 }
            }, 0.25, YAHOO.util.Easing.easeIn);

            webserver.switch_status_div.innerHTML = '<p><span style="background:#33ff00"> ' + o.responseText + ' </span></p>';

            /* enable after fade int */
            result_fade_in.onComplete.subscribe(function() {
                /* Re-enable the form. */
                for(var i=0; i<webserver.switch_form.elements.length; i++) {
                    webserver.switch_form.elements[i].disabled = false;
                };
            });

            result_fade_in.animate();
        },

        failure: function(o) {
            // Set up the animation on the results div.
            var result_fade_in = new YAHOO.util.Anim(webserver.switch_status_div, {
                opacity: { to: 1 }
            }, 0.25, YAHOO.util.Easing.easeIn);

            webserver.switch_status_div.innerHTML = '<p><span style="background:#fb6767"> ERROR </span></p>';

            /* enable after fade int */
            result_fade_in.onComplete.subscribe(function() {
                /* Re-enable the form. */
                for(var i=0; i<webserver.switch_form.elements.length; i++) {
                    webserver.switch_form.elements[i].disabled = false;
                };
            });

            result_fade_in.animate();
        },

        timeout: 3000
    },

    temp_submit: function(e) {
        YAHOO.util.Event.preventDefault(e);
        YAHOO.util.Connect.setForm(webserver.temp_form);

        /* make a XMLHTTP request */
        YAHOO.util.Connect.asyncRequest('POST', '/cmd/tempxhr', webserver.temp_callback);

        var result_fade_out = new YAHOO.util.Anim(webserver.temp_results_div,
            {opacity: { to: 0 }},
            0.25, YAHOO.util.Easing.easeOut);
        result_fade_out.animate();
    }
};

YAHOO.util.Event.addListener(window, 'load', webserver.init);

function popup(url) {
    newwindow = window.open(url, 'image', 'height=500,width=800,resizeable=1,scrollbards,menubar=0,toolbar=0');
    if (window.focus) {newwindow.focus()}
    return false;
}
