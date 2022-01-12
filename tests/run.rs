use std::{env, fs, path::PathBuf, process::Command};

use lang_tester::LangTester;
use lazy_static::lazy_static;
use regex::{Regex, RegexBuilder};
use tempfile::TempDir;

lazy_static! {
    static ref EXPECTED: Regex = RegexBuilder::new(r"^/\*(.*?)^\*/.*$")
        .multi_line(true)
        .dot_matches_new_line(true)
        .build()
        .unwrap();
}

fn main() {
    let tempdir = TempDir::new().unwrap();
    LangTester::new()
        .test_dir(".")
        .test_file_filter(|p| {
            p.extension().map(|x| x.to_str().unwrap()) == Some("c")
            // skipping tests composed of multiple files
                && p.parent().unwrap().file_name().unwrap().to_str() == Some("tests")
        })
        .test_extract(|p| {
            EXPECTED
                .captures(&fs::read_to_string(p).unwrap())
                .map(|x| x.get(1).unwrap().as_str().trim().to_owned())
                .unwrap()
        })
        .test_cmds(move |p| {
            let mut bc = PathBuf::new();
            bc.push(&tempdir);
            bc.push(format!("{}.bc", p.file_stem().unwrap().to_str().unwrap()));

            let mut split_res = PathBuf::new();
            split_res.push(&tempdir);
            split_res.push(p.file_stem().unwrap());

            println!("{}", p.to_str().unwrap());
            let mut get_ir = Command::new(env::var("CC").unwrap_or("clang".to_string()));
            get_ir.args(&[
                "-c",
                "-emit-llvm",
                p.to_str().unwrap(),
                "-o",
                bc.to_str().unwrap(),
            ]);

            let mut make_dir = Command::new("mkdir");
            make_dir.args(&["-v", "-p", split_res.to_str().unwrap()]);

            let mut split = Command::new(
                fs::canonicalize("../split-llvm-extract")
                    .map(|cmd_p| String::from(cmd_p.to_str().unwrap()))
                    .unwrap(),
            );

            split.args(&[bc.to_str().unwrap(), "-o", split_res.to_str().unwrap()]);
            split.current_dir(split_res.clone());

            let mut makefile = Command::new("cp");
            makefile.args(&["-v", "Makefile", split_res.to_str().unwrap()]);

            let mut build = Command::new("make");
            build.current_dir(split_res.clone());

            let mut run = Command::new("./program");
            run.current_dir(split_res.clone());
            run.env("LD_LIBRARY_PATH", split_res.to_str().unwrap());

            vec![
                ("get-ir", get_ir),
                ("make-dir", make_dir),
                ("split", split),
                ("move-makefile", makefile),
                ("build", build),
                ("run", run),
            ]
        })
        .run();
}
