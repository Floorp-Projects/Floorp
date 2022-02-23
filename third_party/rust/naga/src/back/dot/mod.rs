/*! GraphViz (DOT) backend
 *
 * This backend writes a graph in the DOT format, for the ease
 * of IR inspection and debugging.
!*/

use crate::{
    arena::Handle,
    valid::{FunctionInfo, ModuleInfo},
};

use std::{
    borrow::Cow,
    fmt::{Error as FmtError, Write as _},
};

#[derive(Default)]
struct StatementGraph {
    nodes: Vec<&'static str>,
    flow: Vec<(usize, usize, &'static str)>,
    dependencies: Vec<(usize, Handle<crate::Expression>, &'static str)>,
    emits: Vec<(usize, Handle<crate::Expression>)>,
    calls: Vec<(usize, Handle<crate::Function>)>,
}

impl StatementGraph {
    fn add(&mut self, block: &[crate::Statement]) -> usize {
        use crate::Statement as S;
        let root = self.nodes.len();
        self.nodes.push(if root == 0 { "Root" } else { "Node" });
        for statement in block {
            let id = self.nodes.len();
            self.flow.push((id - 1, id, ""));
            self.nodes.push(""); // reserve space
            self.nodes[id] = match *statement {
                S::Emit(ref range) => {
                    for handle in range.clone() {
                        self.emits.push((id, handle));
                    }
                    "Emit"
                }
                S::Break => "Break",       //TODO: loop context
                S::Continue => "Continue", //TODO: loop context
                S::Kill => "Kill",         //TODO: link to the beginning
                S::Barrier(_flags) => "Barrier",
                S::Block(ref b) => {
                    let other = self.add(b);
                    self.flow.push((id, other, ""));
                    "Block"
                }
                S::If {
                    condition,
                    ref accept,
                    ref reject,
                } => {
                    self.dependencies.push((id, condition, "condition"));
                    let accept_id = self.add(accept);
                    self.flow.push((id, accept_id, "accept"));
                    let reject_id = self.add(reject);
                    self.flow.push((id, reject_id, "reject"));
                    "If"
                }
                S::Switch {
                    selector,
                    ref cases,
                } => {
                    self.dependencies.push((id, selector, "selector"));
                    for case in cases {
                        let case_id = self.add(&case.body);
                        let label = match case.value {
                            crate::SwitchValue::Integer(_) => "case",
                            crate::SwitchValue::Default => "default",
                        };
                        self.flow.push((id, case_id, label));
                    }
                    "Switch"
                }
                S::Loop {
                    ref body,
                    ref continuing,
                } => {
                    let body_id = self.add(body);
                    self.flow.push((id, body_id, "body"));
                    let continuing_id = self.add(continuing);
                    self.flow.push((body_id, continuing_id, "continuing"));
                    "Loop"
                }
                S::Return { value } => {
                    if let Some(expr) = value {
                        self.dependencies.push((id, expr, "value"));
                    }
                    "Return"
                }
                S::Store { pointer, value } => {
                    self.dependencies.push((id, value, "value"));
                    self.emits.push((id, pointer));
                    "Store"
                }
                S::ImageStore {
                    image,
                    coordinate,
                    array_index,
                    value,
                } => {
                    self.dependencies.push((id, image, "image"));
                    self.dependencies.push((id, coordinate, "coordinate"));
                    if let Some(expr) = array_index {
                        self.dependencies.push((id, expr, "array_index"));
                    }
                    self.dependencies.push((id, value, "value"));
                    "ImageStore"
                }
                S::Call {
                    function,
                    ref arguments,
                    result,
                } => {
                    for &arg in arguments {
                        self.dependencies.push((id, arg, "arg"));
                    }
                    if let Some(expr) = result {
                        self.emits.push((id, expr));
                    }
                    self.calls.push((id, function));
                    "Call"
                }
                S::Atomic {
                    pointer,
                    ref fun,
                    value,
                    result,
                } => {
                    self.emits.push((id, result));
                    self.dependencies.push((id, pointer, "pointer"));
                    self.dependencies.push((id, value, "value"));
                    if let crate::AtomicFunction::Exchange { compare: Some(cmp) } = *fun {
                        self.dependencies.push((id, cmp, "cmp"));
                    }
                    "Atomic"
                }
            };
        }
        root
    }
}

#[allow(clippy::manual_unwrap_or)]
fn name(option: &Option<String>) -> &str {
    match *option {
        Some(ref name) => name,
        None => "",
    }
}

/// set39 color scheme from <https://graphviz.org/doc/info/colors.html>
const COLORS: &[&str] = &[
    "white", // pattern starts at 1
    "#8dd3c7", "#ffffb3", "#bebada", "#fb8072", "#80b1d3", "#fdb462", "#b3de69", "#fccde5",
    "#d9d9d9",
];

fn write_fun(
    output: &mut String,
    prefix: String,
    fun: &crate::Function,
    info: Option<&FunctionInfo>,
) -> Result<(), FmtError> {
    enum Payload<'a> {
        Arguments(&'a [Handle<crate::Expression>]),
        Local(Handle<crate::LocalVariable>),
        Global(Handle<crate::GlobalVariable>),
    }

    writeln!(output, "\t\tnode [ style=filled ]")?;

    for (handle, var) in fun.local_variables.iter() {
        writeln!(
            output,
            "\t\t{}_l{} [ shape=hexagon label=\"{:?} '{}'\" ]",
            prefix,
            handle.index(),
            handle,
            name(&var.name),
        )?;
    }

    let mut edges = crate::FastHashMap::<&str, _>::default();
    let mut payload = None;
    for (handle, expression) in fun.expressions.iter() {
        use crate::Expression as E;
        let (label, color_id) = match *expression {
            E::Access { base, index } => {
                edges.insert("base", base);
                edges.insert("index", index);
                ("Access".into(), 1)
            }
            E::AccessIndex { base, index } => {
                edges.insert("base", base);
                (format!("AccessIndex[{}]", index).into(), 1)
            }
            E::Constant(_) => ("Constant".into(), 2),
            E::Splat { size, value } => {
                edges.insert("value", value);
                (format!("Splat{:?}", size).into(), 3)
            }
            E::Swizzle {
                size,
                vector,
                pattern,
            } => {
                edges.insert("vector", vector);
                (format!("Swizzle{:?}", &pattern[..size as usize]).into(), 3)
            }
            E::Compose { ref components, .. } => {
                payload = Some(Payload::Arguments(components));
                ("Compose".into(), 3)
            }
            E::FunctionArgument(index) => (format!("Argument[{}]", index).into(), 1),
            E::GlobalVariable(h) => {
                payload = Some(Payload::Global(h));
                ("Global".into(), 2)
            }
            E::LocalVariable(h) => {
                payload = Some(Payload::Local(h));
                ("Local".into(), 1)
            }
            E::Load { pointer } => {
                edges.insert("pointer", pointer);
                ("Load".into(), 4)
            }
            E::ImageSample {
                image,
                sampler,
                gather,
                coordinate,
                array_index,
                offset: _,
                level,
                depth_ref,
            } => {
                edges.insert("image", image);
                edges.insert("sampler", sampler);
                edges.insert("coordinate", coordinate);
                if let Some(expr) = array_index {
                    edges.insert("array_index", expr);
                }
                match level {
                    crate::SampleLevel::Auto => {}
                    crate::SampleLevel::Zero => {}
                    crate::SampleLevel::Exact(expr) => {
                        edges.insert("level", expr);
                    }
                    crate::SampleLevel::Bias(expr) => {
                        edges.insert("bias", expr);
                    }
                    crate::SampleLevel::Gradient { x, y } => {
                        edges.insert("grad_x", x);
                        edges.insert("grad_y", y);
                    }
                }
                if let Some(expr) = depth_ref {
                    edges.insert("depth_ref", expr);
                }
                let string = match gather {
                    Some(component) => Cow::Owned(format!("ImageGather{:?}", component)),
                    _ => Cow::Borrowed("ImageSample"),
                };
                (string, 5)
            }
            E::ImageLoad {
                image,
                coordinate,
                array_index,
                index,
            } => {
                edges.insert("image", image);
                edges.insert("coordinate", coordinate);
                if let Some(expr) = array_index {
                    edges.insert("array_index", expr);
                }
                if let Some(expr) = index {
                    edges.insert("index", expr);
                }
                ("ImageLoad".into(), 5)
            }
            E::ImageQuery { image, query } => {
                edges.insert("image", image);
                let args = match query {
                    crate::ImageQuery::Size { level } => {
                        if let Some(expr) = level {
                            edges.insert("level", expr);
                        }
                        Cow::from("ImageSize")
                    }
                    _ => Cow::Owned(format!("{:?}", query)),
                };
                (args, 7)
            }
            E::Unary { op, expr } => {
                edges.insert("expr", expr);
                (format!("{:?}", op).into(), 6)
            }
            E::Binary { op, left, right } => {
                edges.insert("left", left);
                edges.insert("right", right);
                (format!("{:?}", op).into(), 6)
            }
            E::Select {
                condition,
                accept,
                reject,
            } => {
                edges.insert("condition", condition);
                edges.insert("accept", accept);
                edges.insert("reject", reject);
                ("Select".into(), 3)
            }
            E::Derivative { axis, expr } => {
                edges.insert("", expr);
                (format!("d{:?}", axis).into(), 8)
            }
            E::Relational { fun, argument } => {
                edges.insert("arg", argument);
                (format!("{:?}", fun).into(), 6)
            }
            E::Math {
                fun,
                arg,
                arg1,
                arg2,
                arg3,
            } => {
                edges.insert("arg", arg);
                if let Some(expr) = arg1 {
                    edges.insert("arg1", expr);
                }
                if let Some(expr) = arg2 {
                    edges.insert("arg2", expr);
                }
                if let Some(expr) = arg3 {
                    edges.insert("arg3", expr);
                }
                (format!("{:?}", fun).into(), 7)
            }
            E::As {
                kind,
                expr,
                convert,
            } => {
                edges.insert("", expr);
                let string = match convert {
                    Some(width) => format!("Convert<{:?},{}>", kind, width),
                    None => format!("Bitcast<{:?}>", kind),
                };
                (string.into(), 3)
            }
            E::CallResult(_function) => ("CallResult".into(), 4),
            E::AtomicResult { .. } => ("AtomicResult".into(), 4),
            E::ArrayLength(expr) => {
                edges.insert("", expr);
                ("ArrayLength".into(), 7)
            }
        };

        // give uniform expressions an outline
        let color_attr = match info {
            Some(info) if info[handle].uniformity.non_uniform_result.is_none() => "fillcolor",
            _ => "color",
        };
        writeln!(
            output,
            "\t\t{}_e{} [ {}=\"{}\" label=\"{:?} {}\" ]",
            prefix,
            handle.index(),
            color_attr,
            COLORS[color_id],
            handle,
            label,
        )?;

        for (key, edge) in edges.drain() {
            writeln!(
                output,
                "\t\t{}_e{} -> {}_e{} [ label=\"{}\" ]",
                prefix,
                edge.index(),
                prefix,
                handle.index(),
                key,
            )?;
        }
        match payload.take() {
            Some(Payload::Arguments(list)) => {
                write!(output, "\t\t{{")?;
                for &comp in list {
                    write!(output, " {}_e{}", prefix, comp.index())?;
                }
                writeln!(output, " }} -> {}_e{}", prefix, handle.index())?;
            }
            Some(Payload::Local(h)) => {
                writeln!(
                    output,
                    "\t\t{}_l{} -> {}_e{}",
                    prefix,
                    h.index(),
                    prefix,
                    handle.index(),
                )?;
            }
            Some(Payload::Global(h)) => {
                writeln!(
                    output,
                    "\t\tg{} -> {}_e{} [fillcolor=gray]",
                    h.index(),
                    prefix,
                    handle.index(),
                )?;
            }
            None => {}
        }
    }

    let mut sg = StatementGraph::default();
    sg.add(&fun.body);
    for (index, label) in sg.nodes.into_iter().enumerate() {
        writeln!(
            output,
            "\t\t{}_s{} [ shape=square label=\"{}\" ]",
            prefix, index, label,
        )?;
    }
    for (from, to, label) in sg.flow {
        writeln!(
            output,
            "\t\t{}_s{} -> {}_s{} [ arrowhead=tee label=\"{}\" ]",
            prefix, from, prefix, to, label,
        )?;
    }
    for (to, expr, label) in sg.dependencies {
        writeln!(
            output,
            "\t\t{}_e{} -> {}_s{} [ label=\"{}\" ]",
            prefix,
            expr.index(),
            prefix,
            to,
            label,
        )?;
    }
    for (from, to) in sg.emits {
        writeln!(
            output,
            "\t\t{}_s{} -> {}_e{} [ style=dotted ]",
            prefix,
            from,
            prefix,
            to.index(),
        )?;
    }
    for (from, function) in sg.calls {
        writeln!(
            output,
            "\t\t{}_s{} -> f{}_s0",
            prefix,
            from,
            function.index(),
        )?;
    }

    Ok(())
}

pub fn write(module: &crate::Module, mod_info: Option<&ModuleInfo>) -> Result<String, FmtError> {
    use std::fmt::Write as _;

    let mut output = String::new();
    output += "digraph Module {\n";

    writeln!(output, "\tsubgraph cluster_globals {{")?;
    writeln!(output, "\t\tlabel=\"Globals\"")?;
    for (handle, var) in module.global_variables.iter() {
        writeln!(
            output,
            "\t\tg{} [ shape=hexagon label=\"{:?} {:?}/'{}'\" ]",
            handle.index(),
            handle,
            var.class,
            name(&var.name),
        )?;
    }
    writeln!(output, "\t}}")?;

    for (handle, fun) in module.functions.iter() {
        let prefix = format!("f{}", handle.index());
        writeln!(output, "\tsubgraph cluster_{} {{", prefix)?;
        writeln!(
            output,
            "\t\tlabel=\"Function{:?}/'{}'\"",
            handle,
            name(&fun.name)
        )?;
        let info = mod_info.map(|a| &a[handle]);
        write_fun(&mut output, prefix, fun, info)?;
        writeln!(output, "\t}}")?;
    }
    for (ep_index, ep) in module.entry_points.iter().enumerate() {
        let prefix = format!("ep{}", ep_index);
        writeln!(output, "\tsubgraph cluster_{} {{", prefix)?;
        writeln!(output, "\t\tlabel=\"{:?}/'{}'\"", ep.stage, ep.name)?;
        let info = mod_info.map(|a| a.get_entry_point(ep_index));
        write_fun(&mut output, prefix, &ep.function, info)?;
        writeln!(output, "\t}}")?;
    }

    output += "}\n";
    Ok(output)
}
