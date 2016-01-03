var elixir = require('laravel-elixir');
require('laravel-elixir-vueify');

var sassOpts = { 
        includePaths: [
            'node_modules/breakpoint-sass/stylesheets/',
            'node_modules/normalize-scss/sass/',
            'node_modules/support-for/sass/'
            ] 
        };

elixir(function(mix) {
    mix.sass('app.scss','public/css/app.css', sassOpts)
        .sass('vendor.scss', 'public/css/vendor.css', sassOpts);
   
    mix.browserify('main.js');

    mix.copy('resources/assets/img', 'public/img')
        .copy('resources/assets/fonts/Lato/*.ttf', 'public/fonts')
        .copy('resources/assets/fonts/Montserrat/*.ttf', 'public/fonts')
        .copy('resources/assets/fonts/Source_Sans_Pro/*.ttf', 'public/fonts');

    mix.version([
        'css/app.css', 
        'css/vendor.css', 
        'js/main.js'
    ]);

    mix.browserSync({
        proxy: 'fpp2.dev'
        })
});