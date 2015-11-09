//Xmas2015 YALP playlist + custom Vixen2 mapping

'use strict';

//require('colors');
//require('longjohn'); //http://www.mattinsler.com/post/26396305882/announcing-longjohn-long-stack-traces-for-nodejs

var Playlist = require('my-projects/shared/playlist'); //base class
var xmas = module.exports = new Playlist({autoplay: true});
//xmas.name = "Xmas";
//xmas.volume = 1.0;

//xmas.addSong('my-projects/songs/xmas/Amaz*');
//xmas.addSong('my-projects/songs/xmas/*Capital*');
//xmas.addSong('my-projects/songs/xmas/*Wookie*');
xmas.songs =
[
    "my-projects/songs/xmas/Amaz*",
//    "my-projects/songs/xmas/*Capital*",
//    "my-projects/songs/xmas/*Wookie*",
//    "Hippo",
];
xmas.opening = "thx"; //TODO
xmas.closing = "goodnight"; //TODO


const AM = 0, PM = 1200;
xmas.schedule =
[
    {
        name: 'testing',
        day_from: 1101, //mmdd
        day_to: 1127, //mmdd
        time_from: 751, //0 +PM, //hhmm
        time_to: 2400 +PM, //hhmm
    },
    {
        name: 'before',
        day_from: 1128, //mmdd
        day_to: 1224, //mmdd
        time_from: 530 +PM, //hhmm
        time_to: [ 1100 +PM, 930 +PM, 930 +PM, 930 +PM, 930 +PM, 1100 +PM, 1100 +PM, ], //hhmm by weekday
    },
    {
        name: 'Xmas eve',
        day_from: 1224, //mmdd
        day_to: 1224, //mmdd
        time_from: 500 +PM, //hhmm
        time_to: 1100 +PM, //hhmm
    },
    {
        name: 'between',
        day_from: 1125, //mmdd
        day_to: 1231, //mmdd
        time_from: 530 +PM, //hhmm
        time_to: [ 1100 +PM, 930 +PM, 930 +PM, 930 +PM, 930 +PM, 1100 +PM, 1100 +PM, ], //hhmm by weekday
    },
    {
        name: 'NY eve',
        day_from: 1231, //mmdd
        day_to: 1231, //mmdd
        time_from: 500 +PM, //hhmm
        time_to: 1200 +PM, //hhmm
    },
    {
        name: 'after',
        day_from: 101, //mmdd
        day_to: 104, //mmdd
        time_from: 530 +PM, //hhmm
        time_to: [ 1100 +PM, 930 +PM, 930 +PM, 930 +PM, 930 +PM, 1100 +PM, 1100 +PM, ], //hhmm by weekday
    },
];

//playlist.ready(); //allow caller to use it now
//module.exports.debug();
xmas.debug();


//eof
