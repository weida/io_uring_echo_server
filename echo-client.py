import asyncio
import socket

async def echo_client(host, port, message, client_id):
    reader, writer = await asyncio.open_connection(host, port)

    # Send the message
    writer.write(message.encode())
    await writer.drain()

    # Receive the echoed message
    data = await reader.read(512)
    print(f"Client {client_id}: Received: {data.decode()}")

    # Close the connection
    writer.close()

async def main():
    host = 'localhost'  # Replace with the actual server host
    port = 1234         # Replace with the actual server port
    message = "Hello, server!"

    # Create a list of tasks for 1000 clients
    tasks = [echo_client(host, port, message, i) for i in range(1000)]

    # Execute the tasks concurrently
    await asyncio.gather(*tasks)

if __name__ == "__main__":
    asyncio.run(main())

