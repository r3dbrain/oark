/*
Copyright (c) <2010> <Dreg aka David Reguera Garcia, dreg@fr33project.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "driverusr.h"

void * IOCTLReadKernMem( HANDLE device, READ_KERN_MEM_t * read_kern_mem )
{
	ULONG bytes_read ;

	bytes_read = 0 ;

	if
	( 
		DeviceIoControl
		( 
			device,
			OARK_IOCTL_CHANGE_MODE,
			read_kern_mem,
			sizeof( * read_kern_mem ),
			NULL,
			0,
			& bytes_read,
			( LPOVERLAPPED) NULL
		)
	)
		return read_kern_mem->dst_address;

	return NULL;
}

BOOLEAN LoadDriver( HANDLE * device )
{
	char * full_temp_path;
	SC_HANDLE handle_manager;
	SC_HANDLE handle_service;
	BOOLEAN returnf = FALSE;
	BOOLEAN aux_return;
	DWORD last_error;
	SERVICE_STATUS service_status; 

	* device = NULL;

	if ( GetFullTempPath( & full_temp_path, DRIVER_NAME ) )
	{
		if ( debug )
			printf( " OK: Temp driver full path: %s\n", full_temp_path );

		if ( DumpRSRC( full_temp_path, IDR_OARK_DRIVER, "OARK_DRIVER" ) )
		{
			if ( debug )
				printf( " OK: Dumping RSRC\n" );

			handle_manager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
			if ( handle_manager != NULL )
			{
				if ( debug )
					printf( " OK: OpenSCManager SC_MANAGER_ALL_ACCESS\n" );

				handle_service = \
					CreateServiceA \
					(	
						handle_manager,
						SERVICE_NAME,
						SERVICE_NAME,
						SERVICE_ALL_ACCESS,
						SERVICE_KERNEL_DRIVER,
						SERVICE_DEMAND_START,
						SERVICE_ERROR_NORMAL,
						full_temp_path,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL
					);

				last_error  = GetLastError();
				if ( ( last_error == ERROR_SERVICE_EXISTS ) || ( last_error == ERROR_SERVICE_MARKED_FOR_DELETE ) )
				{
					if ( debug )
						printf( " OK: ERROR_SERVICE_EXISTS / ERROR_SERVICE_MARKED_FOR_DELETE, Opening the service..\n" );

					handle_service = OpenServiceA( handle_manager, SERVICE_NAME, SERVICE_ALL_ACCESS );

					if ( ( last_error == ERROR_SERVICE_MARKED_FOR_DELETE ) && ( handle_service != NULL ) )
					{
						if ( debug )
							printf( " OK: ERROR_SERVICE_MARKED_FOR_DELETE, Stoping and recreating the service..\n" );
						
						ControlService( handle_service, SERVICE_CONTROL_STOP, & service_status );

						CloseServiceHandle( handle_service );

						handle_service = \
							CreateServiceA \
							(	
								handle_manager,
								SERVICE_NAME,
								SERVICE_NAME,
								SERVICE_ALL_ACCESS,
								SERVICE_KERNEL_DRIVER,
								SERVICE_DEMAND_START,
								SERVICE_ERROR_NORMAL,
								full_temp_path,
								NULL,
								NULL,
								NULL,
								NULL,
								NULL
							);
					}
				}
				if ( handle_service != NULL )
				{
					if ( debug )
						printf( " OK: Creating service\n" );

					aux_return = StartService( handle_service, 0, NULL );
					last_error  = GetLastError();
					if ( ( aux_return == TRUE ) || ( last_error == ERROR_SERVICE_ALREADY_RUNNING ) )
					{
						if ( debug )
						{
							printf( " OK: Starting service " );
							if ( last_error == ERROR_SERVICE_ALREADY_RUNNING )
								printf( "was ERROR_SERVICE_ALREADY_RUNNING" );
							putchar( '\n' );
						}

						 * device = \
							CreateFileA \
							( 
								NAMEOF_DEVICE,
								GENERIC_READ | GENERIC_WRITE,
								0, 
								NULL,
								OPEN_EXISTING,
								0,
								NULL
							);

						if( * device != INVALID_HANDLE_VALUE )
						{
							if ( debug )
								printf( " OK: Get the handle to device: %s\n", NAMEOF_DEVICE );

							returnf = TRUE;
						}
						else
						{
							fprintf( stderr, " Error: Get the handle to device: %s", NAMEOF_DEVICE );
							if ( GetLastError() == ERROR_ACCESS_DENIED )
								fprintf( stderr, " , Maybe there are other instance with a handle to device!!" );
							fprintf( stderr, "\n" );
						}
					}
					else
						fprintf( stderr, " Error: Starting service\n" );

					CloseServiceHandle( handle_service );
				}
				else
					fprintf( stderr, " Error: Creating / OpenService (already exist) service\n" );

				CloseServiceHandle( handle_manager );
			}
			else
				fprintf( stderr, " Error: OpenSCManager SC_MANAGER_ALL_ACCESS\n" );

			DeleteFileA( full_temp_path );
		}
		else
			fprintf( stderr, " Error: Dumping RSRC\n" );

		free( full_temp_path );
	}
	else
		fprintf( stderr, " Error: GetFullTempPath\n" );


	return returnf;;
}

int UnloadDriver( HANDLE * device )
{
	SC_HANDLE handle_manager;
	SC_HANDLE handle_service;
	SERVICE_STATUS service_status; 
	BOOLEAN returnf = FALSE;
	BOOLEAN aux_return;
	DWORD last_error;

	CloseHandle( * device );

	handle_manager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

	if ( handle_manager != NULL )
	{
		handle_service = OpenServiceA( handle_manager, SERVICE_NAME, SERVICE_ALL_ACCESS );
		last_error = GetLastError();
		if ( last_error != ERROR_SERVICE_DOES_NOT_EXIST )
		{
			if ( handle_service != NULL )
			{
				if ( debug )
					printf( " OK: OpenSCManager SC_MANAGER_ALL_ACCESS\n" );

				aux_return = ControlService( handle_service, SERVICE_CONTROL_STOP, & service_status );
				last_error = GetLastError();
				if ( aux_return == TRUE || ( last_error == ERROR_SERVICE_NOT_ACTIVE ) )
				{
					if ( debug )
						printf( " OK: Stopping the driver\n" );
					
					aux_return = DeleteService( handle_service );
					last_error = GetLastError();
					if ( ( aux_return == TRUE ) || ( last_error == ERROR_SERVICE_MARKED_FOR_DELETE ) )
					{
						if ( debug )
							printf( " OK: Deleting the service\n" );

						returnf = TRUE;
					}
					else
						fprintf( stderr, " Error: Deleting the service\n" );
				}
				else
					fprintf( stderr, " Error: Stopping the driver\n" );

				CloseServiceHandle( handle_service );
			}
			else
				fprintf( stderr, " Error: OpenService SERVICE_ALL_ACCESS\n" );
		}
		else
		{
			returnf = TRUE;

			if ( debug )
				printf( " OK: OpenService ERROR_SERVICE_DOES_NOT_EXIST\n" ); 
		}
		
		CloseServiceHandle( handle_manager );
	}
	else
		fprintf( stderr, " Error: OpenSCManager SC_MANAGER_ALL_ACCESS\n" );

	return returnf;
}