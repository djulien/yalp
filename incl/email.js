#!/usr/bin/env node

'use strict'; //find bugs easier
//var cfg = global.CFG;
//var cfg = require('my-plugins/cmdline').email; //process command line options and config settings
// /*var sprintf =*/ require('sprintf.js'); //.sprintf;
const {sprintf} = require('sprintf-js'); //https://www.npmjs.com/package/sprintf-js
//console.log("cfg", cfg);
//http://javascript.tutorialhorizon.com/2015/07/02/send-email-node-js-express/
//https://github.com/andris9/nodemailer-smtp-transport#usage
const {nodemailer} = require('nodemailer');
//var colors = require('colors');

//module.exports = cfg? emailer: function(ignored) {}; //commonjs
//var thing = {email: cfg || {}};
//console.log("opts", thing);
//require('credentials')(thing);


//var router = express.Router();
//app.use('/sayHello', router);
//router.post('/', handleSayHello); // handle the route at yourdomain.com/sayHello
//var text = 'Hello world from \n\n' + 'me'; //req.body.name;

//function handleSayHello(req, res) {
    // Not the movie transporter!
//https://github.com/andris9/nodemailer-smtp-transport#usage
if (cfg)
{
    var transporter = nodemailer.createTransport(cfg.transport);

    if (cfg.debug)
        transporter.on('log', function (data)
        {
//            console.log(("email log: " + data).blue, data);
            console.log("email log: %s msg %s".blue, data.type, data.message);
        });
}
else console.error("no email configured");


function emailer(title, body)
{
    if (arguments.length > 2) //body.match(/%[ds]/))
        body = sprintf.apply(arguments); //null, Array.from/*prototype.slice.call*/(arguments).slice(1)); //exclude title
    var is_html = body.match(/[<>]/); //assume html if tags present
    var mailOptions =
    {
        from: cfg.from,
        to: cfg.to, //can be comma-separated list
        subject: title || 'Hi from YALP',
        get [is_html? 'html': 'text']() { return body; }, //plaintext
    };

    transporter.sendMail(mailOptions, function(err, info)
    {
        if (err)
        {
            console.log(("email ERROR: " + err).red);
//            res.json({yo: 'error'});
        }
        else
        {
            console.log(('email Message sent: ' + info.response).green);
//            res.json({yo: info.response});
        }
    });
}

//eof