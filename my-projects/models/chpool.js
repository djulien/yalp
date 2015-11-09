//generic channel pool (hardware port abstraction)


module.exports = ChannelPool;

function ChannelPool(opts)
{
    if (!(this instanceof ChannelPool)) return new (ChannelPool.bind.apply(ChannelPool, [null].concat(Array.from(arguments))))(); //http://stackoverflow.com/questions/1606797/use-of-apply-with-new-operator-is-this-possible
    var add_prop = function(name, value) //expose prop but leave it read-only
    {
//        console.log("this is ", this, this.constructor.name, this.constructor + '');
//        if (thing[name]) return; //already there
        Object.defineProperty(this, name, {value: value});
//        console.log("extended %s with %s".blue, thing.constructor.name, name);
    }.bind(this);

    add_prop('opts', (typeof opts !== 'object')? {name: opts}: opts || {});
    add_prop('name', this.opts.name || 'UNKNOWN');
    var m_last_adrs = 0;
    add_prop('last_adrs', function() { return m_last_adrs; });
    this.getadrs = function(count)
    {
        if (typeof count === 'undefined') count = 1; //default but allow caller to specify 0
        return m_last_adrs += count;
    }
    var m_numch = 0;
    add_prop('numch', function() { return m_numch; });
    this.getch = function(count)
    {
        if (typeof count === 'undefined') count = 16; //default but allow caller to specify 0
        return (m_numch += count) - count;
    }
    var m_buf = null; //CAUTION: delay alloc until all ch counts known
    add_prop('buf', function()
    {
        if (!m_numch) throw "Chpool: no channels allocated";
        console.log("chpool: %s buf len %d", m_numch, m_buf? "return": "alloc");
        if (!m_buf) m_buf = new Buffer(m_numch);
        return m_buf;
    });
    this.alloc = function(model, opts)
    {
//    debugger;
        if (m_buf) throw "Channel pool buffer already allocated."; //{ console.log("Enlarging channel buffer"); m_buf = null; }
//        var m_opts = (typeof opts !== 'object')? {numch: opts}: opts || {};
//        m_opts.chpool = this;
////        ++chpool.last_adrs;
//        var retval = new model(m_opts); //{adrs: chpool.last_adrs, startch: chpool.numch, getbuf: chpool.getbuf});
////        chpool.numch += numch;
//        return retval;
        opts = (typeof opts !== 'object')? {first_param: opts}: opts || {};
        if (!(this instanceof ChannelPool)) throw "Don't call ChannelPool.alloc with \"new\"";
        opts.chpool = this; //NOTE: "this" needs to refer to parent ChannelPool here
        var args = Array.prototype.slice.call(arguments, 1); args[0] = opts; //Array.from(arguments); args.shift()
//        console.log("alloc model args", args);
        return model.apply(null, args);
    }

    if (!ChannelPool.all) ChannelPool.all = [];
    ChannelPool.all.push(this); //this.all = function(cb) { m_allinst.forEach(function(ctlr, inx) { cb(ctlr, inx); }); }
}


//eof
