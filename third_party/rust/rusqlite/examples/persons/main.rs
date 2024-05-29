extern crate rusqlite;
use rusqlite::{Connection, Result};

struct Person {
    id: i32,
    name: String,
}
fn main() -> Result<()> {
    let conn = Connection::open_in_memory()?;

    conn.execute(
        "CREATE TABLE IF NOT EXISTS persons (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL
        )",
        (), // empty list of parameters.
    )?;

    conn.execute(
        "INSERT INTO persons (name) VALUES (?1), (?2), (?3)",
        ["Steven", "John", "Alex"].map(|n| n.to_string()),
    )?;

    let mut stmt = conn.prepare("SELECT id, name FROM persons")?;
    let rows = stmt.query_map([], |row| {
        Ok(Person {
            id: row.get(0)?,
            name: row.get(1)?,
        })
    })?;

    println!("Found persons:");

    for person in rows {
        match person {
            Ok(p) => println!("ID: {}, Name: {}", p.id, p.name),
            Err(e) => eprintln!("Error: {e:?}"),
        }
    }

    Ok(())
}
