let dumpTypes = options['dump-types'].split(',');
function interestingType(name) dumpTypes.some(function(n) n == name);

let typelist = {};

function addSubtype(t, subt)
{
  if (subt.typedef === undefined &&
      subt.kind === undefined)
    throw Error("Unexpected subtype: not class or typedef: " + subt);

  if (t.subtypes === undefined)
    t.subtypes = [];
  
  t.subtypes.push(subt);
}

function process_type(t)
{
  let name = t.name;
  
  if (interestingType(t.name))
    typelist[t.name] = t;
  
  if (t.memberOf)
    addSubtype(t.memberOf, t);
}

function process_decl(d)
{
  if (d.typedef !== undefined && d.memberOf)
    addSubtype(d.memberOf, d);
}

function publicBases(t)
{
  yield t;

  for each (let base in t.bases)
    if (base.access == "public")
      for each (let gbase in publicBases(base.type))
        yield gbase;
}

function publicMembers(t)
{
  for each (let base in publicBases(t)) {
    for each (let member in base.members) {
      if (member.access === undefined)
        throw Error("Harumph: member without access? " + member);

      if (member.access != "public")
        continue;
      
      yield member;
    }
  }
}

function signaturesMatch(m1, m2)
{
  let p1 = m1.type.parameters;
  let p2 = m2.type.parameters;
  
  if (p1.length != p2.length)
    return false;
  
  for (let i = 0; i < p1.length; ++i)
    if (p1[i] !== p2[i])
      return false;
  
  return true;
}

/**
 * Get the short name of a decl name. E.g. turn
 * "MyNamespace::MyClass::Method(int j) const" into
 * "Method"
 */
function getShortName(decl)
{
  let name = decl.name;
  let lp = name.lastIndexOf('(');
  if (lp != -1)
    name = name.slice(0, lp);
  
  lp = name.lastIndexOf('::');
  if (lp != -1)
    name = name.slice(lp + 2);

  return name;
}

/**
 * Remove functions in a base class which were overridden in a derived
 * class.
 *
 * Although really, we should perhaps do this the other way around, or even
 * group the two together, but that can come later.
 */ 
function removeOverrides(members)
{
  let overrideMap = {};
  for (let i = members.length - 1; i >= 0; --i) {
    let m = members[i];
    if (!m.isFunction)
      continue;

    let shortName = getShortName(m);

    let overrides = overrideMap[shortName];
    if (overrides === undefined) {
      overrideMap[shortName] = [m];
      continue;
    }

    let found = false;
    for each (let override in overrides) {
      if (signaturesMatch(override, m)) {
        // remove members[i], it was overridden
        members.splice(i, 1);
        found = true;
      }
    }
    if (found)
      continue;
         
    overrides.push(m);
  }
}

/**
 * Generates the starting position of lines within a file.
 */
function getLineLocations(fdata)
{
  yield 0;
  
  let r = /\n/y;
  let pos = 0;
  let i = 1;
  for (;;) {
    pos = fdata.indexOf('\n', pos) + 1;
    if (pos == 0)
      break;

    yield pos;
    i++;
  }
}
    
/**
 * Find and return the doxygen comment immediately prior to the location
 * object that was passed in.
 * 
 * @todo: parse doccomment data such as @param, @returns
 * @todo: parse comments for markup
 */
function getDocComment(loc)
{
  let fdata = read_file(loc.file);
  let linemap = [l for (l in getLineLocations(fdata))];
  
  if (loc.line >= linemap.length) {
    warning("Location larger than actual header: " + loc);
    return <></>;
  }
  
  let endpos = linemap[loc.line - 1] + loc.column - 1;
  let semipos = fdata.lastIndexOf(';', endpos);
  let bracepos = fdata.lastIndexOf('}', endpos);
  let searchslice = fdata.slice(Math.max(semipos, bracepos) + 1, endpos);

  let m = searchslice.match(/\/\*\*[\s\S]*?\*\//gm);
  if (m === null)
    return <></>;
  
  let dc = m[m.length - 1].slice(3, -2);
  dc = dc.replace(/^\s*(\*+[ \t]*)?/gm, "");

  return <pre class="doccomment">{dc}</pre>;
}

function typeName(t)
{
  if (t.name !== undefined)
    return t.name;

  if (t.isPointer)
    return "%s%s*".format(t.isConst ? "const " : "", typeName(t.type));
  
  if (t.isReference)
    return "%s%s&".format(t.isConst ? "const " : "", typeName(t.type));

  return t.toString();
}

function publicBaseList(t)
{
  let l = <ul/>;
  for each (let b in t.bases) {
    if (b.access == 'public')
      l.* += <li><a href={"/en/%s".format(b.type.name)}>{b.type.name}</a></li>;
  }

  if (l.*.length() == 0)
    return <></>;
  
  return <>
    <h2>Base Classes</h2>
    {l}
  </>;
}

/**
 * Get a source-link for a given location.
 */
function getLocLink(loc, text)
{
  return <a class="loc"
            href={"http://hg.mozilla.org/mozilla-central/file/%s/[LOC%s]#l%i".format(options.rev, loc.file, loc.line)}>{text}</a>;
}

function dumpType(t)
{
  let methodOverview = <tbody />;
  let methodList = <div/>;
  let memberList = <></>;

  let shortNameMap = {};

  let members = [m for (m in publicMembers(t))];
  
  removeOverrides(members);

  for each (let m in members) {
    let qname = m.memberOf.name + '::';

    // we don't inherit constructors from base classes
    if (m.isConstructor && m.memberOf !== t)
      continue;
    
    if (m.name.indexOf(qname) != 0)
      throw Error("Member name not qualified?");
    
    let name = m.name.slice(qname.length);
    
    if (name.indexOf('~') == 0)
      continue;

    if (m.isFunction) {
      let innerList;

      let shortName = getShortName(m);
      if (m.isConstructor)
        shortName = 'Constructors';

      if (shortNameMap.hasOwnProperty(shortName)) {
        innerList = shortNameMap[shortName];
      }
      else {
        let overview = 
          <tr><td>
            <a href={'#%s'.format(escape(shortName))}>{shortName}</a>
          </td></tr>;

        if (m.isConstructor)
          methodOverview.insertChildAfter(null, overview);
        else
          methodOverview.appendChild(overview);
        
        let shortMarkup =
          <div>
            <h3 id={shortName}>{shortName}</h3>
            <dl/>
          </div>;

        
        if (m.isConstructor)
          methodList.insertChildAfter(null, shortMarkup);
        else
          methodList.appendChild(shortMarkup);

        innerList = shortMarkup.dl;
        shortNameMap[shortName] = innerList;
      }
      
      let parameters = <ul/>;
      for each (p in m.parameters) {
        let name = p.name;
        if (name == 'this')
          continue;
        
        if (/^D_\d+$/.test(name))
          name = '<anonymous>';
        
        parameters.* += <li>{typeName(p.type)} {name}</li>;
      }

      innerList.* +=
        <>
          <dt id={name} class="methodName">
            <code>{typeName(m.type.type)} {name}</code> - {getLocLink(m.loc, "source")}
          </dt>
          <dd>
            {getDocComment(m.loc)}
            {parameters.*.length() > 0 ?
             <>
               <h4>Parameters</h4>
               {parameters}
             </> : <></>}
          </dd>
        </>;
    }
    else {
      memberList += <li class="member">{name}</li>;
    }
  }

  let r =
    <body>
      <p>{getLocLink(t.loc, "Class Declaration")}</p>
  
      {getDocComment(t.loc)}
  
      {methodOverview.*.length() > 0 ?
         <>
           <h2>Method Overview</h2>
           <table class="standard-table">{methodOverview}</table>
         </> :
         ""
      }

      {publicBaseList(t)}
  
      <h2>Data Members</h2>

      {memberList.*.length() > 0 ?
         memberList :
         <p><em>No public members.</em></p>
      }

      <h2>Methods</h2>
  
      {methodList.*.length() > 0 ?
         methodList :
         <p><em>No public methods.</em></p>
      }
  
    </body>;

  write_file(t.name + ".html", r.toXMLString());
}

function input_end()
{
  for (let p in typelist)
    dumpType(typelist[p]);
}
