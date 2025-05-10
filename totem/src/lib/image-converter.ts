/**
 * Converts an image to a buffer format suitable for an LED matrix display
 * @param imageFile The image file to convert (from file input)
 * @returns Uint32Array containing the pixel data in the format expected by the LED matrix
 */
export async function convertImageToLedMatrixBuffer(imageFile: File): Promise<Uint32Array<ArrayBuffer>> {
  // Create a URL for the uploaded file
  const imageUrl = URL.createObjectURL(imageFile);

  try {
    // Load the image
    const img = await loadImage(imageUrl);

    // Set the target size
    const targetSize = { width: 64, height: 64 };

    // Calculate the aspect ratios
    const imgRatio = img.width / img.height;
    const targetRatio = targetSize.width / targetSize.height;

    // Calculate the scaling factor based on the aspect ratios
    let scaleFactor: number;
    if (imgRatio > targetRatio) {
      // Image is wider than the target size
      scaleFactor = targetSize.height / img.height;
    } else {
      // Image is taller than the target size or has the same aspect ratio
      scaleFactor = targetSize.width / img.width;
    }

    // Calculate the new size after scaling
    const newSize = {
      width: Math.floor(img.width * scaleFactor),
      height: Math.floor(img.height * scaleFactor),
    };

    // Create a canvas for resizing
    const canvas = document.createElement('canvas');
    canvas.width = targetSize.width;
    canvas.height = targetSize.height;
    const ctx = canvas.getContext('2d')!;

    // Fill with black background
    ctx.fillStyle = 'black';
    ctx.fillRect(0, 0, targetSize.width, targetSize.height);

    // Calculate position to center the image
    const left = Math.floor((targetSize.width - newSize.width) / 2);
    const top = Math.floor((targetSize.height - newSize.height) / 2);

    // Draw the image onto the canvas with proper scaling
    ctx.drawImage(img, 0, 0, img.width, img.height, left, top, newSize.width, newSize.height);

    // Get the pixel data
    const imageData = ctx.getImageData(0, 0, targetSize.width, targetSize.height);
    const pixels = imageData.data;

    // Initialize the buffer
    const buffer = new Uint32Array(targetSize.width * targetSize.height);

    // Convert each pixel to RGB format and store in the buffer
    for (let y = 0; y < targetSize.height; y++) {
      for (let x = 0; x < targetSize.width; x++) {
        const i = (y * targetSize.width + x) * 4; // RGBA data index

        // Extract RGB values
        const r = pixels[i];
        const g = pixels[i + 1];
        const b = pixels[i + 2];

        // Convert to 32-bit format: RRGGBB (same as the Python version)
        buffer[y * targetSize.width + x] = (r << 16) | (g << 8) | b;
      }
    }

    return buffer;
  } finally {
    // Clean up the URL object
    URL.revokeObjectURL(imageUrl);
  }
}

/**
 * Helper function to load an image and wait for it to be fully loaded
 */
function loadImage(url: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = reject;
    img.src = url;
  });
}

/**
 * Converts the buffer to a Uint8Array suitable for sending to the ESP32
 */
export function convertBufferToByteArray(buffer: Uint32Array): Uint8Array {
  // Create a Uint8Array with 4 bytes per 32-bit value
  const byteArray = new Uint8Array(buffer.length * 4);

  // Copy the data byte by byte (little-endian)
  for (let i = 0; i < buffer.length; i++) {
    const value = buffer[i];
    const offset = i * 4;

    byteArray[offset] = value & 0xff; // LSB
    byteArray[offset + 1] = (value >> 8) & 0xff;
    byteArray[offset + 2] = (value >> 16) & 0xff;
    byteArray[offset + 3] = (value >> 24) & 0xff; // MSB
  }

  return byteArray;
}

/**
 * Processes a GIF file to extract frames and convert them to LED matrix format
 * @param gifFile The GIF file to process
 * @param onProgress Optional callback for progress updates
 * @returns Promise resolving to an array of frame buffers
 */
export async function processGifToFrames(
  gifFile: File,
  onProgress?: (progress: number) => void,
): Promise<Uint32Array<ArrayBuffer>> {
  return new Promise<Uint32Array<ArrayBuffer>>((resolve, reject) => {
    // Create URL for the GIF file
    const gifUrl = URL.createObjectURL(gifFile);

    // Load the GIF.js library dynamically if not already loaded
    if (!(window as any).SuperGif) {
      const script = document.createElement('script');
      script.src = 'https://cdn.jsdelivr.net/npm/libgif@0.0.3/libgif.min.js';
      script.onload = () => processGif();
      script.onerror = () => reject(new Error('Failed to load GIF processing library'));
      document.head.appendChild(script);
    } else {
      processGif();
    }

    function processGif() {
      try {
        // Create hidden elements to process the GIF
        const container = document.createElement('div');
        container.style.position = 'absolute';
        container.style.opacity = '0';
        container.style.pointerEvents = 'none';
        container.style.zIndex = '-1000';
        document.body.appendChild(container);

        // Create image element
        const img = document.createElement('img');
        img.src = gifUrl;
        container.appendChild(img);

        // Create canvas for processing frames
        const canvas = document.createElement('canvas');
        canvas.width = 64;
        canvas.height = 64;
        const ctx = canvas.getContext('2d')!;

        // Initialize SuperGif
        const gif = new (window as any).SuperGif({
          gif: img,
          auto_play: false,
        });

        gif.load(() => {
          try {
            const frameCount = gif.get_length();
            const pixelsPerFrame = 64 * 64; // 64x64 matrix

            // Create a single large buffer to hold all frames
            const combinedBuffer = new Uint32Array(pixelsPerFrame * frameCount);

            // Process each frame
            for (let i = 0; i < frameCount; i++) {
              // Go to specific frame
              gif.move_to(i);

              // Get frame canvas
              const frameCanvas = gif.get_canvas();

              // Process the frame and add to combined buffer
              processSingleFrame(frameCanvas, canvas, ctx, combinedBuffer, i * pixelsPerFrame);

              // Report progress if callback provided
              if (onProgress) {
                onProgress((i + 1) / frameCount);
              }
            }

            // Clean up
            document.body.removeChild(container);
            URL.revokeObjectURL(gifUrl);

            resolve(combinedBuffer);
          } catch (err) {
            reject(err);
          }
        });
      } catch (err) {
        URL.revokeObjectURL(gifUrl);
        reject(err);
      }
    }
  });
}

/**
 * Process a single frame from a GIF
 */
function processSingleFrame(
  sourceCanvas: HTMLCanvasElement,
  targetCanvas: HTMLCanvasElement,
  ctx: CanvasRenderingContext2D,
  combinedBuffer: Uint32Array,
  bufferOffset: number,
): void {
  const targetSize = { width: 64, height: 64 };

  // Calculate aspect ratios
  const imgRatio = sourceCanvas.width / sourceCanvas.height;
  const targetRatio = targetSize.width / targetSize.height;

  // Calculate scaling factor
  let scaleFactor: number;
  if (imgRatio > targetRatio) {
    scaleFactor = targetSize.height / sourceCanvas.height;
  } else {
    scaleFactor = targetSize.width / sourceCanvas.width;
  }

  // Calculate new size
  const newSize = {
    width: Math.floor(sourceCanvas.width * scaleFactor),
    height: Math.floor(sourceCanvas.height * scaleFactor),
  };

  // Clear canvas with black background
  ctx.fillStyle = 'black';
  ctx.fillRect(0, 0, targetSize.width, targetSize.height);

  // Center the image
  const left = Math.floor((targetSize.width - newSize.width) / 2);
  const top = Math.floor((targetSize.height - newSize.height) / 2);

  // Draw the frame
  ctx.drawImage(
    sourceCanvas,
    0,
    0,
    sourceCanvas.width,
    sourceCanvas.height,
    left,
    top,
    newSize.width,
    newSize.height,
  );

  // Get pixel data
  const imageData = ctx.getImageData(0, 0, targetSize.width, targetSize.height);
  const pixels = imageData.data;

  // Convert to buffer format and add to the combined buffer at specified offset
  for (let y = 0; y < targetSize.height; y++) {
    for (let x = 0; x < targetSize.width; x++) {
      const pixelIndex = y * targetSize.width + x;
      const dataIndex = pixelIndex * 4;

      const r = pixels[dataIndex];
      const g = pixels[dataIndex + 1];
      const b = pixels[dataIndex + 2];

      // Store directly in the combined buffer at the right offset
      combinedBuffer[bufferOffset + pixelIndex] = (r << 16) | (g << 8) | b;
    }
  }
}
