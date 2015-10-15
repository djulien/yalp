var playback = require('my-plugins/utils/ipc')("playlist");
var iomon = require('my-plugins/utils/ipc')("iostats");

var seen = 0, prev = 0;
playback.on(function(data_req)
{
//	message.reply({'you':'got it'})
	seen = data.frame;
//	console.log("rcv: data ", message, data);
});

//listener.on('reset', function(req, data)
//{
//	seen = pev = 0;
//});


setInterval(function()
{
	iostats.send({frame: seen, });
	if (seen != prev) console.log("last seen: %d (+%d)", seen, seen - prev);
	prev = seen;
}, 1000);
