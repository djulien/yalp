//http://expressjs.com/guide/routing.html
//http://expressjs.com/guide/using-middleware.html

var numroutes = 0;
global.seqnum = 0; //mainly for trace/debug convenience

var path = require('path');
var require_glob = require('node-glob-loader').load; //https://www.npmjs.com/package/node-glob-loader

module.exports = setup; //commonjs
//console.log("route dirname ", __dirname);

var loop = 0;
var server = null; //NOTE: also called by socketio.listen
function setup(app)
{
//no    var server = null; //NOTE: also called by socketio.listen
    console.log("no server %d", loop++); //if (loop > 1) throw "called too often";
    if (server) return;
    var applisten = app.listen;
//    console.log("app.listen = " + applisten);
    app.listen = function(port, host, cb) //kludge: grab http server when created by express and reuse for socket io; should occur first since require_glob is async
    {
        server = applisten.apply(app, arguments); //creates http server
        console.log("route index: got server?", !!server);
        return server;
    }.bind(app);
//    require_glob(__dirname + '/**/*[!-bk].js', {strict: true, noself: true}, function(exported, filename)
    require_glob(__dirname + '/**/!(*-bk).js', {strict: true, noself: true}, function(exported, filename)
    {
//        var relpath = path.relative(__dirname /*path.dirname(require.main.filename)*/, filename);
        var method = path.relative(__dirname, filename).split(path.sep, 1)[0]; //parent folder == http method
//        console.log("route", filename, __filename);
//        if (path.basename(filename) == path.basename(__filename)) return; //skip self
        console.log("route[%d] %s %s '%s'".blue, numroutes++, method, exported.uri || '(any)', path.relative(path.dirname(require.main.filename), filename)); //, exported);
//        if (path.basename(filename) == path.basename(__filename)) return; //skip self
//        if ("socket,process".indexOf(method) != -1) //exported(app);
        if (!app[method])
        {
            console.log("starting socket io with server? %d", !!server);
            if (!server) throw "Tried to open non-http route before http server created";
            exported(server); //reuse http server for socket io; http://stackoverflow.com/questions/17696801/express-js-app-listen-vs-server-listen/17697134#17697134
        }
        else if (exported.uri) app[method](exported.uri, exported.handler);
        else app[method](exported.handler);
    })./*done*/ then(function() { console.log("routes loaded: %d".green, numroutes); });

    app.on('close', function ()
    {
        console.log("closed".red);
    //    redis.quit();
    });
}

//console.log("TODO: routes".red);
//module.exports = function() {}

//eof
