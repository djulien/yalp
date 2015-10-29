//See the great tutorial at https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial
//fillRect(x, y, width, height); //Draws a filled rectangle.
//strokeRect(x, y, width, height); //Draws a rectangular outline.
//clearRect(x, y, width, height); //Clears the specified rectangular area, making it fully transparent.

//ctx.strokeStyle = "#ddd";
//ctx.fillStyle = "rgb(255,255,255)";

//var $ = require('jquery');
//var canvas = document.getElementById("fx-canvas");
//var ctx = canvas.getContext("2d");

//NOTE: better perf with int coords; use: rounded = (0.5 + somenum) | 0;
function Model(opts)
{
    if (!(this instanceof Model)) return new Model(opts);
    if (!opts) opts = {};
    var m_image = {}; //= make_canvas();
    var m_view = {}; //= make_canvas(true);
    this.isModel = true;

//canvas properties:
//canvas is cleared when these are changed
    Object.defineProperties(this,
    {
        w:
        {
            get() { return m_image.w || 0; },
            set(neww)
            {
                console.log("set w to %d", neww);
                if (neww == m_image.w) return;
                resized.call(this);
                m_image.w = neww;
                if (m_view.canvas) m_view.canvas.style.width = (m_view.canvas.width = this.scale * neww + 1) + 'px'; //NOTE: +1 for last grid line
            },
        },
        h:
        {
            get() { return m_image.h || 0; },
            set(newh)
            {
                console.log("set h to %d", newh);
                if (newh == m_image.h) return;
                resized.call(this);
                m_image.h = newh;
                if (m_view.canvas) m_view.canvas.style.height = (m_view.canvas.height = this.scale * newh + 1) + 'px';
            },
        },
        canvas:
        {
            get()
            {
                if (!m_image.canvas)
                {
                    m_image.canvas = make_canvas.call(this);
                    m_image.canvas.width = m_image.w;
                    m_image.canvas.height = m_image.h;
                    m_image.context = null;
                }
                return m_image.canvas;
            },
        },
        context_ro:
        {
            get() { return m_image.context; },
        },
        context:
        {
            get()
            {
                if (!m_image.context)
                {
                    m_image.context = this.canvas.getContext("2d");
//                    m_image['1x1'] = m_image.context.createImageData(1, 1); ////http://stackoverflow.com/questions/4899799/whats-the-best-way-to-set-a-single-pixel-in-an-html5-canvas
//                    m_image.pixel  = m_image['1x1'].data;
                }
                this.dirty = true; //mark for redraw (assume caller wants context in order to draw on it)
                return m_image.context;
            }
        }
    });
    function resized()
    {
        console.log("resized: canvas? %s, w %s, h %s, model? %s", !!m_image.canvas, this.w, this.h, this.isModel);
        m_image.canvas = null;
        m_image.context = null;
//        m_image['1x1'] = null;
//        m_image.pixel = null;
        moved.call(this);
    }

//view properties:
//canvas is preserved but needs redraw when these are changed
//    var m_pos = {};
    Object.defineProperties(this,
    {
        x:
        {
            get() { return m_view.x || 0; },
            set(newx)
            {
                console.log("set x to %d", newx);
                if (newx == m_view.x) return;
                moved.call(this);
                m_view.x = newx;
                if (m_view.canvas) m_view.canvas.style.left = newx + 'px';
            },
        },
        y:
        {
            get() { return m_view.y || 0; },
            set(newy)
            {
                console.log("set y to %d", newy);
                if (newy == m_view.y) return;
                moved.call(this);
                m_view.y = newy;
                if (m_view.canvas) m_view.canvas.style.top = newy + 'px';
            },
        },
        scale:
        {
            get() { return m_view.scale || 1.0; },
            set(newscale)
            {
                console.log("set scale to %d", newscale);
                if (newscale == m_view.scale) return;
                moved.call(this);
                m_view.scale = newscale;
                if (!m_view.canvas) return;
                m_view.canvas.style.width = (m_view.canvas.width = newscale * this.w + 1) + 'px'; //NOTE: +1 for last grid line
                m_view.canvas.style.height = (m_view.canvas.height = newscale * this.h + 1) + 'px';
            },
        },
        dirty:
        {
            get() { return m_view.dirty || false; },
            set(newdirty)
            {
                console.log("dirty: was %s, is now %s", !!m_view.dirty, newdirty);
                if (newdirty && !m_view.dirty) m_view.dirty = setTimeout(draw.bind(this), 10); //pending redraw
                else if (!newdirty && m_view.dirty) { clearTimeout(m_view.dirty); m_view.dirty = null; } //cancel pending redraw
            },
        },
    });
//    var m_drawn; //m_prior, m_pending;
    function moved()
    {
//        if (!m_drawn) return;
//        if (!m_view.drawn) return;
//        this.clear(); //erase old position first
//        m_view.drawn = false;
//        this.dirty(); //mark for redraw
//        m_previous = {x: undefined, y: undefined, scale: undefined};
//        redraw(); //erase from screen before moving
        console.log("moved + cleared: drawn before? %s, w %s, h %s, scale %s, model? %s", m_view.context && m_view.drawn, this.w, this.h, this.scale, this.isModel);
        if (!m_view.context || !m_view.drawn) return; //not on screen yet
        m_view.context.clearRect(0, 0, this.w * this.scale + 1, this.h * this.scale + 1); //xparent
        m_view.drawn = false;
        this.dirty = true; //needs redraw in new position
    }

//methods:
    this.clear = function(color)
    {
//        var TEMP = true; //false;
        var ctx = /*TEMP? m_view.context:*/ this.context; //NOTE: marks view dirty
        ctx.save();
//        if (TEMP) ctx.scale(this.scale, this.scale);
        if (!arguments.length) //|| (alpha(color) <= 0))
        {
            ctx.clearRect(0, 0, this.w, this.h); //xparent
//no            this.m_view.drawn = false;
        }
        else
        {
            if (color) ctx.fillStyle = color; //"#000000"; //stylewhen filling shapes
//    ctx.strokeStyle = "#000000"; //style for shapes' outlines
            ctx.fillRect(0, 0, this.w, this.h);
        }
        ctx.restore();
//        if (!TEMP) this.draw();
//        this.dirty(); //mark for redraw
    }

    this.pixel = function(x, y, color) //, bypass)//TODO
    {
        if (arguments.length > 2) //set; //http://stackoverflow.com/questions/4899799/whats-the-best-way-to-set-a-single-pixel-in-an-html5-canvas
        {
            var ctx = this.context;
            ctx.save();
            ctx.fillStyle = color; //"rgba("+r+","+g+","+b+","+(a/255)+")";
            ctx.fillRect(x, y, 1, 1);
            ctx.restore();
        }
        else //get; http://stackoverflow.com/questions/667045/getpixel-from-html-canvas
        {
            var ctx = this.context_ro;
            if (!ctx) return 0;
            var data = ctx.getImageData(x, y, 1, 1).data;
//            color = new Color([data[0], data[1], data[2]]);            
//            return data[0]
            var uint32array = new Uint32Array(data, 0, 1);
            return uint32array[0];
        }
    }

//helpers:
    function draw() //first)
    {
//        var first = false;
//        if (first || !m_view.canvas)
        console.log("draw, canvas? %s, drawn? %s, model? %s", !!m_view.canvas, m_view.drawn, this.isModel);
        if (!m_view.canvas)
        {
            m_view.canvas = make_canvas.call(this, true);
            m_view.context = m_view.canvas.getContext("2d");
            m_view.drawn = false; //first = true;
        }
        m_view.context.save();
        if (!m_view.drawn) //first) //draw grid
        {
            m_view.context.beginPath();
            m_view.context.strokeStyle = '#080';
            for (var x = 0.5; x < m_view.canvas.width; x += this.scale) //NOTE: + 0.5 for integral pixel boundaries
            {
                m_view.context.moveTo(x, 0);
                m_view.context.lineTo(x, m_view.canvas.height);
            }
            for (var y = 0.5; y < m_view.canvas.height; y += this.scale)
            {
                m_view.context.moveTo(0, y);
                m_view.context.lineTo(m_view.canvas.width, y);
            }
            m_view.context.stroke();
//            m_view.context.restore();
        }
//        m_view.context.save();
        console.log("scale to %d and copy image", this.scale);
        m_view.context.scale(this.scale, this.scale); //only scale when copying image
        if (false) m_view.context.drawImage(m_image.canvas, 0, 0); //TODO: no worky
        var ctx = this.context_ro;
        if (ctx)
        {
            var imageData = ctx.getImageData(0, 0, this.w, this.h); //kludge: this is the slow way, but the faster way gives an error
            console.log("img data to draw? %s: len %d ", !!imageData, imageData.length, imageData);
            m_view.context.putImageData(imageData, 0, 0); //, this.w * this.scale, this.h * this.scale);
        }
        m_view.context.restore();
        m_view.drawn = true; //remember something is on screen
        this.dirty = false;
    }

    function make_canvas(add_dom)
    {
        var canvas = document.createElement('canvas');
//            $('<canvas id="model-' + ++Model.seqnum + '" width="160" height="160">').appendTo('body');
//            m_view = document.getElementById('#model-' + Model.seqnum);
//http://stackoverflow.com/questions/10652513/html5-dynamically-create-canvas
        if (add_dom) //make it displayable
        {
            if (!Model.seqnum) Model.seqnum = 0;
            canvas.id = "model-" + Model.seqnum++;
//            m_view.width  = 160;
//            m_view.height = 160;
//            m_view.style.zIndex   = 8;
//            m_view.style.position = "absolute";
//            m_view.style.border   = "1px solid";
            canvas.className = 'fx-canvas';
//            first = true;
            canvas.style.top = this.y + 'px';
            canvas.style.left = this.x + 'px';
            canvas.style.width = (canvas.width = this.scale * this.w + 1) + 'px'; //NOTE: +1 for last grid line
            canvas.style.height = (canvas.height = this.scale * this.h + 1) + 'px';
            console.log("cre canvas: x %d %s, y %d %s, w %d %s, h %d %s", this.x, canvas.style.left, this.y, canvas.style.top, this.w, canvas.style.width, this.h, canvas.style.height);
//            console.log("created canvas view# %s %s", canvas.id, Model.seqnum);
            document.body.appendChild(canvas);
        }
        else console.log("cre off-screen canvas");
        return canvas;
    }

    function alpha(color)
    {
        return 1.0; //TODO
    }

//object initialization:
    this.w = opts.w || 16;
    this.h = opts.h || 16;
    this.scale = opts.scale || 10;
    this.x = opts.x || Math.max($(document).width() - this.scale * this.w, 0);
//    console.log("x = ", opts.x, this.x, $(document).width(), this.scale, this.w);
    this.y = opts.y || 10;
//    this.clear('#f00');
    draw.call(this); //true);

    if (!Model.allInst) Model.allInst = [];
    Model.allInst.push(this);
    Model.count = Model.allInst.length; //kludge; getter on Model not working, so just set a static value
    console.log("model ctor: now there are %d", Model.count);
}
Model.count = 0;
//Object.defineProperties(Model,
//{
//    count: { get() { return (Model.allinst || []).length; }, },
//});
Model.all = function(cb)
{
    (Model.allInst || []).forEach(function(model, inx)
    {
        cb(model, inx);
    });
}

function show(model, inx)
{
    console.log("%d models:", Model.count);
    Model.all(function(model, inx)
    {
        console.log("model%d/%d]: x %d, y %d, w %d, h %d", inx, Model.count, model.x, model.y, model.w, model.h);
    });
}


var m = Model({w: 3, h: 2, y: 250, x:550});
//var m2 = Model({x:400, y:200});
//var m3 = Model({w: 8, h: 4, x: 500, y: 20});
//show();
//m.x = 450; m.y += 40;
m.clear('#ff0');

if (false)
setTimeout(function()
{
    console.clear();
    var ctx = m.context_ro;
    console.log("img data 1: ", ctx.getImageData(0, 0, m.w, m.h), m.pixel(0, 0), m.pixel(1, 1), m.pixel(2, 0));
    m.pixel(0, 0, '#f00');
    console.log("img data 2: ", ctx.getImageData(0, 0, m.w, m.h), m.pixel(0, 0), m.pixel(1, 1), m.pixel(2, 0));
    m.pixel(1, 1, '#0F0');
    console.log("img data 3: ", ctx.getImageData(0, 0, m.w, m.h), m.pixel(0, 0), m.pixel(1, 1), m.pixel(2, 0));
    m.pixel(2, 0, '#00f');
    console.log("img data 4: ", ctx.getImageData(0, 0, m.w, m.h), m.pixel(0, 0), m.pixel(1, 1), m.pixel(2, 0));
//    m.clear('#f00');
    show();
}, 3000);

var orange = "#ffa500";
//var m = new Model({w: 48, h: 16, x: 200, y: 20});
console.log("there are %d inst", Model.count);
if (0) m.clearAll(orange);
//window.allModels[0].clear(orange);
if (0) m.context.fillStyle = '#ffa500';
if (0) m.context.fillRect(0, 0, 8, 4);
if (0) m.draw();
//m.x = 10;
//window.m.x = 10;

//        context.strokeStyle = '#f00';
//        context.beginPath();
//        context.moveTo(10, 10);
//        context.lineTo(80, 80);
//        context.stroke();
//canvas.top = '10px';
//canvas.left = '10px';

//var canvas = model.canvas;
//var ctx = model.cx;
//var m = new Model();

if (0) console.clear();
//console.log("w = %d, h = %d", canvas.width, canvas.height);

if (0) model.clear();
if (0) model.clear(orange);
if (0) model.resize(50, 50);
if (0) model.moveto(300, 0);
if (0) model.grid(10, 10, '#0f0');
//model.cx.translate(0, 100);
if (0) model.line(48.3, 42, 48.3, 48, '#0ff', 1);
if (0) { model.w = 48; model.h = 16; model.clear(orange); }
if (0) { model.scale = 12; model.x = 200; model.y = 10; }
if (0) model.grid(48, 16, '#0f0');
if (0) model.line(0, 0, 4, 2, '#f00');
//canvas.width = 480; canvas.height = 160;
if (0) { ctx.fillStyle = orange; ctx.fillRect(0, 0, 1, 1); ctx.fillStyle = 0; }
if (0)
{
    console.log("line", ctx.lineWidth, model.m_scale);
    ctx.beginPath();
    ctx.strokeStyle = '#f0f'; ctx.fillStyle = 0;
    ctx.moveTo(2,1);
    ctx.lineTo(12, 2);
    //ctx.fillRect(1,10,3,3);
    ctx.stroke();
    //ctx.stroke();
}
//model.line(0, 0, 4, 4, '#f00')
//model.w = 480; model.x = 10; model.h = 160;
//model.x = 500;
//model.line(0, 0, 10, 10, '#f0f')
//model.grid(48, 48, '#0f0')
//model.clear()
//model.x = 560;

//clear('#fff');
//clear();
//ctx.lineWidth = 1;
//ctx.miterLimit = 10;
//ctx.globalAlpha = 1.0; //0.2;
//  ctx.shadowOffsetX = 0;
//  ctx.shadowOffsetY = 0;
//  ctx.shadowBlur = 0;
//  ctx.shadowColor = "rgba(0, 0, 0, 0.5)";
//ctx.fillStyle = orange;
//ctx.strokeStyle = '#00f';
//ctx.strokeStyle = orange;
//ctx.fillStyle = "#000";

//test1();
//test2();
//test3();
//test4();
//test5();
//test6();
//test7();
//test8();
//test9();
//test10();
//test11();
//test12();
//test13();
//test14();
//test15();
//test16();
//test17();
//test18();
//test19();
//test20();
//test21();
//test22();
//test23();
//test24();
//test25();
//test26();
//test27();
//test28();
//test29();
//test30();
//test31();
//TODO: https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial/Transformations
//test32();

//grid('#ddd');


//ctx.fillRect (0, 0, canvas.width, canvas.height);


//ctx.moveTo(0,0);
//ctx.lineTo(380,380);

//ctx.stroke();
//alert("done");

function clear(color)
{
    if (!arguments.length) return ctx.clearRect(0, 0, canvas.width, canvas.height); //xparent
    if (color) ctx.fillStyle = color; //"#000000"; //stylewhen filling shapes
//    ctx.strokeStyle = "#000000"; //style for shapes' outlines
    ctx.fillRect(0, 0, canvas.width, canvas.height)
}

function grid(color)
{
    if (color) ctx.strokeStyle = color; //"#ddd"; //style for shapes' outlines

    for (var x = 0; x < canvas.width; x += 10)
    {
      ctx.moveTo(x, 0);
      ctx.lineTo(x, canvas.height);
    }
    
    for (var y = 0; y < canvas.height; y += 10)
    {
      ctx.moveTo(0, y);
      ctx.lineTo(canvas.width, y);
    }
}

//from http://stackoverflow.com/questions/667045/getpixel-from-html-canvas
function getPixel(x, y)
{
    data = ctx.getImageData(x, y, 1, 1).data;
 //   color = new Color([data[0], data[1], data[2]]);
    var color = [data[0], data[1], data[2], data[3]]; // returns array [R,G,B,A]
    return color;
}

//https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial/Pixel_manipulation_with_canvas
function blankImage()
{
    var myImageData = ctx.createImageData(width, height); //all pixels xparent black
//var myImageData = ctx.createImageData(anotherImageData);
    return myImageData;
}

//from http://stackoverflow.com/questions/667045/getpixel-from-html-canvas
function invert()
{
    var imgd = ctx.getImageData(0, 0, canvas.width, canvas.height);
    var pix = imgd.data;

// Loop over each pixel and invert the color.
    for (var i = 0, n = pix.length; i < n; i += 4)
    {
        pix[i  ] = 255 - pix[i  ]; // red
        pix[i+1] = 255 - pix[i+1]; // green
        pix[i+2] = 255 - pix[i+2]; // blue
        // i+3 is alpha (the fourth element)
    }
// Draw the ImageData at the given (x,y) coordinates.
    ctx.putImageData(imgd, x, y);
}

//from http://ejohn.org/blog/ocr-and-neural-nets-in-javascript/
function convert_grey(image_data)
{
    for (var x = 0; x < image_data.width; x++)
        for (var y = 0; y < image_data.height; y++)
        {
            var i = x*4+y*4*image_data.width;
            var luma = Math.floor(image_data.data[i] * 299/1000 + image_data.data[i+1] * 587/1000 + image_data.data[i+2] * 114/1000);
            image_data.data[i] = luma;
            image_data.data[i+1] = luma;
            image_data.data[i+2] = luma;
            image_data.data[i+3] = 255;
        }
}
    
function test1() //rect
{
    ctx.fillStyle = "rgb(200,0,0)";
    ctx.fillRect (10, 10, 55, 50);
    ctx.fillStyle = "rgba(0, 0, 200, 0.5)";
    ctx.fillRect (30, 30, 55, 50);
}

function test2() //path (triangle)
{
   ctx.beginPath();
    ctx.moveTo(75,50);
    ctx.lineTo(100,75);
    ctx.lineTo(100,25);
    ctx.fill();
}

function test3() //pen (smiley face)
{
   ctx.beginPath();
    ctx.arc(75,75,50,0,Math.PI*2,true); // Outer circle
    ctx.moveTo(110,75);
    ctx.arc(75,75,35,0,Math.PI,false);  // Mouth (clockwise)
    ctx.moveTo(65,65);
    ctx.arc(60,65,5,0,Math.PI*2,true);  // Left eye
    ctx.moveTo(95,65);
    ctx.arc(90,65,5,0,Math.PI*2,true);  // Right eye
    ctx.stroke();
}

function test4() //lines
{
  // Filled triangle
    ctx.beginPath();
    ctx.moveTo(25,25);
    ctx.lineTo(105,25);
    ctx.lineTo(25,105);
    ctx.fill();

    // Stroked triangle
    ctx.beginPath();
    ctx.moveTo(125,125);
    ctx.lineTo(125,45);
    ctx.lineTo(45,125);
    ctx.closePath();
    ctx.stroke();
}

function test5() //arcs
{
   for(var i=0;i<4;i++)
   {
      for(var j=0;j<3;j++)
      {
        ctx.beginPath();
        var x = 25+j*50; // x coordinate
        var y = 25+i*50; // y coordinate
        var radius = 20; // Arc radius
        var startAngle = 0; // Starting point on circle
        var endAngle = Math.PI+(Math.PI*j)/2; // End point on circle
        var anticlockwise = i%2==0 ? false : true; // clockwise or anticlockwise

        ctx.arc(x, y, radius, startAngle, endAngle, anticlockwise);

        if (i>1) ctx.fill();
        else ctx.stroke();
      }
   }
}

function test6() //quadratic bezier (speech balloon)
{
    ctx.beginPath();
    ctx.moveTo(75,25);
    ctx.quadraticCurveTo(25,25,25,62.5);
    ctx.quadraticCurveTo(25,100,50,100);
    ctx.quadraticCurveTo(50,120,30,125);
    ctx.quadraticCurveTo(60,120,65,100);
    ctx.quadraticCurveTo(125,100,125,62.5);
    ctx.quadraticCurveTo(125,25,75,25);
    ctx.stroke();
}

function test7() //cubic bezier (heart)
{
    // Quadratric curves example
    ctx.beginPath();
    ctx.moveTo(75,40);
    ctx.bezierCurveTo(75,37,70,25,50,25);
    ctx.bezierCurveTo(20,25,20,62.5,20,62.5);
    ctx.bezierCurveTo(20,80,40,102,75,120);
    ctx.bezierCurveTo(110,102,130,80,130,62.5);
    ctx.bezierCurveTo(130,62.5,130,25,100,25);
    ctx.bezierCurveTo(85,25,75,37,75,40);
    ctx.fill();
}

function test8() //combo path
{
     roundedRect(ctx,12,12,150,150,15);
    roundedRect(ctx,19,19,150,150,9);
    roundedRect(ctx,53,53,49,33,10);
    roundedRect(ctx,53,119,49,16,6);
    roundedRect(ctx,135,53,49,33,10);
    roundedRect(ctx,135,119,25,49,10);

    ctx.beginPath();
    ctx.arc(37,37,13,Math.PI/7,-Math.PI/7,false);
    ctx.lineTo(31,37);
    ctx.fill();

    for(var i=0;i<8;i++)
    {
      ctx.fillRect(51+i*16,35,4,4);
    }

    for(i=0;i<6;i++)
    {
      ctx.fillRect(115,51+i*16,4,4);
    }

    for(i=0;i<8;i++)
    {
      ctx.fillRect(51+i*16,99,4,4);
    }

    ctx.beginPath();
    ctx.moveTo(83,116);
    ctx.lineTo(83,102);
    ctx.bezierCurveTo(83,94,89,88,97,88);
    ctx.bezierCurveTo(105,88,111,94,111,102);
    ctx.lineTo(111,116);
    ctx.lineTo(106.333,111.333);
    ctx.lineTo(101.666,116);
    ctx.lineTo(97,111.333);
    ctx.lineTo(92.333,116);
    ctx.lineTo(87.666,111.333);
    ctx.lineTo(83,116);
    ctx.fill();

    ctx.fillStyle = "white";
    ctx.beginPath();
    ctx.moveTo(91,96);
    ctx.bezierCurveTo(88,96,87,99,87,101);
    ctx.bezierCurveTo(87,103,88,106,91,106);
    ctx.bezierCurveTo(94,106,95,103,95,101);
    ctx.bezierCurveTo(95,99,94,96,91,96);
    ctx.moveTo(103,96);
    ctx.bezierCurveTo(100,96,99,99,99,101);
    ctx.bezierCurveTo(99,103,100,106,103,106);
    ctx.bezierCurveTo(106,106,107,103,107,101);
    ctx.bezierCurveTo(107,99,106,96,103,96);
    ctx.fill();

    ctx.fillStyle = "black";
    ctx.beginPath();
    ctx.arc(101,102,2,0,Math.PI*2,true);
    ctx.fill();

    ctx.beginPath();
    ctx.arc(89,102,2,0,Math.PI*2,true);
    ctx.fill();
}

// A utility function to draw a rectangle with rounded corners.
function roundedRect(ctx,x,y,width,height,radius)
{
  ctx.beginPath();
  ctx.moveTo(x,y+radius);
  ctx.lineTo(x,y+height-radius);
  ctx.quadraticCurveTo(x,y+height,x+radius,y+height);
  ctx.lineTo(x+width-radius,y+height);
  ctx.quadraticCurveTo(x+width,y+height,x+width,y+height-radius);
  ctx.lineTo(x+width,y+radius);
  ctx.quadraticCurveTo(x+width,y,x+width-radius,y);
  ctx.lineTo(x+radius,y);
  ctx.quadraticCurveTo(x,y,x,y+radius);
  ctx.stroke();
}

function test9() //Path2D
{
    var rectangle = new Path2D();
    rectangle.rect(10, 10, 50, 50);

    var circle = new Path2D();
    circle.moveTo(125, 35);
    circle.arc(100, 35, 25, 0, 2 * Math.PI);

    ctx.stroke(rectangle);
    ctx.fill(circle);
}

function test10() //fillStyle
{
  for (var i=0;i<6;i++)
    for (var j=0;j<6;j++)
    {
      ctx.fillStyle = 'rgb(' + Math.floor(255-42.5*i) + ',' + Math.floor(255-42.5*j) + ',0)';
      ctx.fillRect(j*25,i*25,25,25);
    }
}

function test11() //strokeStyle
{
    for (var i=0;i<6;i++)
      for (var j=0;j<6;j++)
      {
        ctx.strokeStyle = 'rgb(0,' + Math.floor(255-42.5*i) + ',' + Math.floor(255-42.5*j) + ')';
        ctx.beginPath();
        ctx.arc(12.5+j*25,12.5+i*25,10,0,Math.PI*2,true);
        ctx.stroke();
      }
}

function test12() //xparencty (globalAlpha)
{
  ctx.fillStyle = '#FD0';
  ctx.fillRect(0,0,75,75);
  ctx.fillStyle = '#6C0';
  ctx.fillRect(75,0,75,75);
  ctx.fillStyle = '#09F';
  ctx.fillRect(0,75,75,75);
  ctx.fillStyle = '#F30';
  ctx.fillRect(75,75,75,75);
  ctx.fillStyle = '#FFF';

  // set transparency value
  ctx.globalAlpha = 0.2;

  // Draw semi transparent circles
  for (i=0;i<7;i++)
  {
    ctx.beginPath();
    ctx.arc(75,75,10+10*i,0,Math.PI*2,true);
    ctx.fill();
  }
}

function test13() //rbga
{
  // Draw background
  ctx.fillStyle = 'rgb(255,221,0)';
  ctx.fillRect(0,0,150,37.5);
  ctx.fillStyle = 'rgb(102,204,0)';
  ctx.fillRect(0,37.5,150,37.5);
  ctx.fillStyle = 'rgb(0,153,255)';
  ctx.fillRect(0,75,150,37.5);
  ctx.fillStyle = 'rgb(255,51,0)';
  ctx.fillRect(0,112.5,150,37.5);

  // Draw semi transparent rectangles
  for (var i=0;i<10;i++)
  {
    ctx.fillStyle = 'rgba(255,255,255,'+(i+1)/10+')';
    for (var j=0;j<4;j++)
      ctx.fillRect(5+i*14,5+j*37.5,14,27.5);
  }
}

function test14() //line width
{
  for (var i = 0; i < 10; i++)
  {
    ctx.lineWidth = 1+i;
    ctx.beginPath();
    ctx.moveTo(5+i*14,5);
    ctx.lineTo(5+i*14,140);
    ctx.stroke();
  }
}    

function test15() //linecap
{
    var lineCap = ['butt','round','square'];
// Draw guides
  ctx.strokeStyle = '#09f';
  ctx.beginPath();
  ctx.moveTo(10,10);
  ctx.lineTo(140,10);
  ctx.moveTo(10,140);
  ctx.lineTo(140,140);
  ctx.stroke();

  // Draw lines
  ctx.strokeStyle = 'black';
  for (var i=0;i<lineCap.length;i++)
  {
    ctx.lineWidth = 15;
    ctx.lineCap = lineCap[i];
    ctx.beginPath();
    ctx.moveTo(25+i*50,10);
    ctx.lineTo(25+i*50,140);
    ctx.stroke();
  }
}

function test16() //line join
{
  var lineJoin = ['round','bevel','miter'];
  ctx.lineWidth = 10;
  for (var i=0;i<lineJoin.length;i++)
  {
    ctx.lineJoin = lineJoin[i];
    ctx.beginPath();
    ctx.moveTo(-5,5+i*40);
    ctx.lineTo(35,45+i*40);
    ctx.lineTo(75,5+i*40);
    ctx.lineTo(115,45+i*40);
    ctx.lineTo(155,5+i*40);
    ctx.stroke();
  }
}

function test17() //miter limit
{
  // Clear canvas
  ctx.clearRect(0,0,150,150);
 
  // Draw guides
  ctx.strokeStyle = '#09f';
  ctx.lineWidth   = 2;
  ctx.strokeRect(-5,50,160,50);
 
  // Set line styles
  ctx.strokeStyle = '#000';
  ctx.lineWidth = 10;
 
  // check input
  if (true) ctx.miterLimit = 10;
  else if (document.getElementById('miterLimit').value.match(/\d+(\.\d+)?/))
    ctx.miterLimit = parseFloat(document.getElementById('miterLimit').value);
  else
    alert('Value must be a positive number');
 
  // Draw lines
  ctx.beginPath();
  ctx.moveTo(0,100);
  for (i=0;i<24;i++)
  {
    var dy = i%2==0 ? 25 : -25 ;
    ctx.lineTo(Math.pow(i,1.5)*2,75+dy);
  }
  ctx.stroke();
  return false;
}

function test18() //line dash
{
    var offset = 0;

    function draw()
    {
      ctx.clearRect(0,0, canvas.width, canvas.height);
      ctx.setLineDash([4, 2]);
      ctx.lineDashOffset = -(offset % 16);
      ctx.strokeRect(10,10, 100, 100);
    }

    function march()
    {
      offset++;
//      if (offset > 16) offset = 0;
      draw();
      if (offset < 5000/20) setTimeout(march, 20);
    }

    march();
}

function test19() //linear gradient
{
  // Create gradients
  var lingrad = ctx.createLinearGradient(0,0,0,150);
  lingrad.addColorStop(0, '#00ABEB');
  lingrad.addColorStop(0.5, '#fff');
  lingrad.addColorStop(0.5, '#26C000');
  lingrad.addColorStop(1, '#fff');

  var lingrad2 = ctx.createLinearGradient(0,50,0,95);
  lingrad2.addColorStop(0.5, '#000');
  lingrad2.addColorStop(1, 'rgba(0,0,0,0)');

  // assign gradients to fill and stroke styles
  ctx.fillStyle = lingrad;
  ctx.strokeStyle = lingrad2;
  
  // draw shapes
  ctx.fillRect(10,10,130,130);
  ctx.strokeRect(50,50,50,50);
}

function test20() //radial gradient
{
 // Create gradients
  var radgrad = ctx.createRadialGradient(45,45,10,52,50,30);
  radgrad.addColorStop(0, '#A7D30C');
  radgrad.addColorStop(0.9, '#019F62');
  radgrad.addColorStop(1, 'rgba(1,159,98,0)');
  
  var radgrad2 = ctx.createRadialGradient(105,105,20,112,120,50);
  radgrad2.addColorStop(0, '#FF5F98');
  radgrad2.addColorStop(0.75, '#FF0188');
  radgrad2.addColorStop(1, 'rgba(255,1,136,0)');

  var radgrad3 = ctx.createRadialGradient(95,15,15,102,20,40);
  radgrad3.addColorStop(0, '#00C9FF');
  radgrad3.addColorStop(0.8, '#00B5E2');
  radgrad3.addColorStop(1, 'rgba(0,201,255,0)');

  var radgrad4 = ctx.createRadialGradient(0,150,50,0,140,90);
  radgrad4.addColorStop(0, '#F4F201');
  radgrad4.addColorStop(0.8, '#E4C700');
  radgrad4.addColorStop(1, 'rgba(228,199,0,0)');
  
  // draw shapes
  ctx.fillStyle = radgrad4;
  ctx.fillRect(0,0,150,150);
  ctx.fillStyle = radgrad3;
  ctx.fillRect(0,0,150,150);
  ctx.fillStyle = radgrad2;
  ctx.fillRect(0,0,150,150);
  ctx.fillStyle = radgrad;
  ctx.fillRect(0,0,150,150);
}

function test21() //createPattern
{
// create new image object to use as pattern
  var img = new Image();
  img.src = 'https://mdn.mozillademos.org/files/222/Canvas_createpattern.png';
  img.onload = function()
  {
    // create pattern
    var ptrn = ctx.createPattern(img,'repeat');
    ctx.fillStyle = ptrn;
    ctx.fillRect(0,0,150,150);

  }
}

function test22() //shadowed text
{
  ctx.shadowOffsetX = 2;
  ctx.shadowOffsetY = 2;
  ctx.shadowBlur = 2;
  ctx.shadowColor = "rgba(0, 0, 0, 0.5)";
 
  ctx.font = "20px Times New Roman";
  ctx.fillStyle = "Black";
  ctx.fillText("Sample String", 5, 30);
}

function test23() //canvas fill rule
{
  ctx.beginPath(); 
  ctx.arc(50, 50, 30, 0, Math.PI*2, true);
  ctx.arc(50, 50, 15, 0, Math.PI*2, true);
  ctx.fill("evenodd");
}

function test24() //fill text
{
  ctx.font = "48px serif";
  ctx.fillText("Hello world", 10, 50);
}

function test25() //stroke text
{
  ctx.font = "48px serif";
  ctx.strokeText("Hello world", 10, 50);
}

function test26() //text baseline (animate)
{
    if (!test26.offset) test26.offset = 30;

    clear('#fff');    
    ctx.font = "48px serif";
    ctx.textBaseline = "hanging";
    ctx.strokeText("Hello world", 0, test26.offset );
    test26.offset += 2;
    if (test26.offset < 140) setTimeout(test26, 100);
}

function test27() //inline image (base64)
{
    var img = new Image();   // Create new img element
    img.src = 'data:image/gif;base64,R0lGODlhCwALAIAAAAAA3pn/ZiH5BAEAAAEALAAAAAALAAsAAAIUhA+hkcuO4lmNVindo7qyrIXiGBYAOw==';
    img.onload = function()
    {
        ctx.drawImage(img,0,0, 50, 50);
    };
}

function test28() //line graph with bkg image
{
  var img = new Image();
  img.onload = function()
  {
      console.log("image is %d x %d", img.width, img.height);
    ctx.drawImage(img,0,0); //, 300, 300);
    ctx.beginPath();
    ctx.moveTo(30,96);
    ctx.lineTo(70,66);
    ctx.lineTo(103,76);
    ctx.lineTo(170,15);
    ctx.stroke();
  };
  img.src = 'https://mdn.mozillademos.org/files/5395/backdrop.png';
}

function test29() //tiling
{
  var img = new Image();
  img.onload = function()
  {
    for (var i=0;i<4;i++)
      for (var j=0;j<3;j++)
        ctx.drawImage(img,j*50,i*38,50,38);
  };
  img.src = 'https://mdn.mozillademos.org/files/5397/rhino.jpg';
}

function test30() //image slice
{
    $('<div style="display:none;"> \
     <img id="source" src="https://mdn.mozillademos.org/files/5397/rhino.jpg" width="300" height="227"> \
     <img id="frame" src="https://mdn.mozillademos.org/files/242/Canvas_picture_frame.png" width="132" height="150"> \
   </div>').appendTo('#javascript-editor');
   setTimeout(function() //or use onload
   {
      // Draw slice
      ctx.drawImage(document.getElementById('source'), 33, 71, 104, 124, 21, 20, 87, 104);
      // Draw frame
      ctx.drawImage(document.getElementById('frame'),0,0);
   }, 100);
}

function test31() //smoothing
{
  var img = new Image();
  img.onload = function()
  {
    ctx.drawImage(img,0, 0, 50, 50, 10, 10, 400, 400);
  };
//  img.src = 'https://mdn.mozillademos.org/files/5397/rhino.jpg';
    img.src = 'data:image/gif;base64,R0lGODlhCwALAIAAAAAA3pn/ZiH5BAEAAAEALAAAAAALAAsAAAIUhA+hkcuO4lmNVindo7qyrIXiGBYAOw==';

//    ctx.mozImageSmoothingEnabled = false;
//    ctx.webkitImageSmoothingEnabled = false;
//    ctx.msImageSmoothingEnabled = false;
    ctx.imageSmoothingEnabled = false;
}

console.log("done!");
//eof

