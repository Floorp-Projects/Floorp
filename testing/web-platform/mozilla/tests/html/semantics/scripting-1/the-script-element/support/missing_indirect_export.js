// Import a non-existent export to trigger instantiation failure.
export {x, not_found} from "./module.js";
