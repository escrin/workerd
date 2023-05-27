export default {
  async fetch(_req, env) {
    const esModule = () => {
      addEventListener("message", (message) => {
        console.log("inner worker got message:", JSON.stringify(message.data));
        postMessage(message.data);
      });
    };
    const worker = new Worker(
      stringToDataUrl(
        esModule.toString().replace(/\s+/g, " ").replace(/^\(\) => /, "")
      )
    );
    worker.onmessage = (msg) => {
      console.log("got a message from worker:", JSON.stringify(msg));
    };
    worker.onerror = (e) => {
      console.error("worker got an error:", JSON.stringify(e));
    };
    worker.postMessage({ message: "hello" });
    return new Response("ok");
  },
};

function stringToDataUrl(str, mimeType = "text/plain") {
  return `data:${mimeType};charset=utf-8,compatibility-date=2023-02-28,${encodeURIComponent(
    str
  )}`;
}
