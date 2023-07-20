use std::collections::HashMap;
use std::path::{Path, PathBuf};

use crate::config::Config;
use crate::parser::{Loop, Macro, Node};
use crate::CompileError;

pub(crate) struct Heritage<'a> {
    pub(crate) root: &'a Context<'a>,
    pub(crate) blocks: BlockAncestry<'a>,
}

impl Heritage<'_> {
    pub(crate) fn new<'n>(
        mut ctx: &'n Context<'n>,
        contexts: &'n HashMap<&'n Path, Context<'n>>,
    ) -> Heritage<'n> {
        let mut blocks: BlockAncestry<'n> = ctx
            .blocks
            .iter()
            .map(|(name, def)| (*name, vec![(ctx, *def)]))
            .collect();

        while let Some(ref path) = ctx.extends {
            ctx = &contexts[path.as_path()];
            for (name, def) in &ctx.blocks {
                blocks.entry(name).or_insert_with(Vec::new).push((ctx, def));
            }
        }

        Heritage { root: ctx, blocks }
    }
}

type BlockAncestry<'a> = HashMap<&'a str, Vec<(&'a Context<'a>, &'a Node<'a>)>>;

pub(crate) struct Context<'a> {
    pub(crate) nodes: &'a [Node<'a>],
    pub(crate) extends: Option<PathBuf>,
    pub(crate) blocks: HashMap<&'a str, &'a Node<'a>>,
    pub(crate) macros: HashMap<&'a str, &'a Macro<'a>>,
    pub(crate) imports: HashMap<&'a str, PathBuf>,
}

impl Context<'_> {
    pub(crate) fn new<'n>(
        config: &Config<'_>,
        path: &Path,
        nodes: &'n [Node<'n>],
    ) -> Result<Context<'n>, CompileError> {
        let mut extends = None;
        let mut blocks = Vec::new();
        let mut macros = HashMap::new();
        let mut imports = HashMap::new();
        let mut nested = vec![nodes];
        let mut top = true;

        while let Some(nodes) = nested.pop() {
            for n in nodes {
                match n {
                    Node::Extends(extends_path) if top => match extends {
                        Some(_) => return Err("multiple extend blocks found".into()),
                        None => {
                            extends = Some(config.find_template(extends_path, Some(path))?);
                        }
                    },
                    Node::Macro(name, m) if top => {
                        macros.insert(*name, m);
                    }
                    Node::Import(_, import_path, scope) if top => {
                        let path = config.find_template(import_path, Some(path))?;
                        imports.insert(*scope, path);
                    }
                    Node::Extends(_) | Node::Macro(_, _) | Node::Import(_, _, _) if !top => {
                        return Err(
                            "extends, macro or import blocks not allowed below top level".into(),
                        );
                    }
                    def @ Node::BlockDef(_, _, _, _) => {
                        blocks.push(def);
                        if let Node::BlockDef(_, _, nodes, _) = def {
                            nested.push(nodes);
                        }
                    }
                    Node::Cond(branches, _) => {
                        for (_, _, nodes) in branches {
                            nested.push(nodes);
                        }
                    }
                    Node::Loop(Loop {
                        body, else_block, ..
                    }) => {
                        nested.push(body);
                        nested.push(else_block);
                    }
                    Node::Match(_, _, arms, _) => {
                        for (_, _, arm) in arms {
                            nested.push(arm);
                        }
                    }
                    _ => {}
                }
            }
            top = false;
        }

        let blocks: HashMap<_, _> = blocks
            .iter()
            .map(|def| {
                if let Node::BlockDef(_, name, _, _) = def {
                    (*name, *def)
                } else {
                    unreachable!()
                }
            })
            .collect();

        Ok(Context {
            nodes,
            extends,
            blocks,
            macros,
            imports,
        })
    }
}
