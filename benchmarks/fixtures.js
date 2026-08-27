const fs = require("fs");
const path = require("path");

function getFixturePaths() {
  return fs
    .readdirSync(path.join(__dirname, "..", "fixtures", "ip-matcher"))
    .filter((file) => file.endsWith(".json"))
    .sort()
    .map((file) => path.join(__dirname, "..", "fixtures", "ip-matcher", file));
}

function readList(fixturePath) {
  const networks = JSON.parse(fs.readFileSync(fixturePath, "utf8"));
  if (
    !Array.isArray(networks) ||
    !networks.every((network) => typeof network === "string")
  ) {
    throw new Error(
      `Expected ${fixturePath} to contain an array of IP networks`,
    );
  }
  return networks;
}

module.exports = { getFixturePaths, readList };
