export default {
  async fetch(_req, env) {
    const esModule = () => {
      addEventListener("message", (message) => {
        console.log("inner worker got message:", JSON.stringify(message.data));
        if (message.data instanceof CryptoKey) {
          postMessage('got a key');
        }
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
    worker.postMessage(await createMinimalCryptoKey());
    worker.postMessage({ message: "hello" });
    return new Response("ok");
  },
};

function stringToDataUrl(str, mimeType = "text/plain") {
  return `data:${mimeType};charset=utf-8,compatibility-date=2023-02-28,${encodeURIComponent(
    str
  )}`;
}

async function createMinimalCryptoKey() {
  const algorithm = { name: 'AES-GCM', length: 256 }; // Example algorithm (AES-GCM with a key length of 256 bits)
  const extractable = true; // The key can be extracted
  const keyUsages = ['encrypt', 'decrypt']; // Example key usages (encrypt and decrypt)

  try {
    const key = await crypto.subtle.generateKey(algorithm, extractable, keyUsages);
    return key;
  } catch (error) {
    console.error('Error creating CryptoKey:', error);
    return null;
  }
}
