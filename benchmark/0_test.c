#define Py_ssize_t int
struct s{
    int link;
};
struct efi_loaded_image {
    int image_base;
    int image_size;
};
struct efi_loaded_image_obj {
 int header;
 int image_type;
 int exit_status;
 int exit_jmp;
};
#define t struct s
#define efi_handle_t t*
#define efi_status_t t*
#define efi_uintn_t t*
#define NULL 0
//efi_handle_t* efi_obj_list;
//PyObject* Py_None;
#define PyExc_ValueError "PyExc_ValueError"
//efi_status_t tmp;
efi_status_t EFI_SUCCESS;
//extern efi_handle_t efi_root;
//extern efi_status_t efi_obj_list_initialized;
#define uintptr_t int*
#define u16 int
efi_handle_t efi_guid_loaded_image;
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL "EFI_OPEN_PROTOCOL_GET_PROTOCOL"
#define IMAGE_SUBSYSTEM_EFI_APPLICATION "IMAGE_SUBSYSTEM_EFI_APPLICATION"
void efi_delete_handle(efi_handle_t handle)
{
	if (!handle)
		return;
	efi_remove_all_protocols(handle);
	list_del(&handle->link);
	free(handle);
}

static efi_status_t efi_delete_image
			(struct efi_loaded_image_obj *image_obj,
			 struct efi_loaded_image *loaded_image_protocol)
{
	struct efi_object *efiobj;
	efi_status_t r, ret = *EFI_SUCCESS;
/*
close_next:
	list_for_each_entry(efiobj, &efi_obj_list, link) {
		struct efi_handler *protocol;

		list_for_each_entry(protocol, &efiobj->protocols, link) {
			struct efi_open_protocol_info_item *info;

			list_for_each_entry(info, &protocol->open_infos, link) {
				if (info->info.agent_handle !=
				    (efi_handle_t)image_obj)
					continue;
				r = EFI_CALL(efi_close_protocol
						(efiobj, protocol->guid,
						 info->info.agent_handle,
						 info->info.controller_handle
						));
				if (r !=  EFI_SUCCESS)
					ret = r;
				goto close_next;
			}
		}
	}
*/
	efi_free_pages((uintptr_t)loaded_image_protocol->image_base,
		       efi_size_in_pages(loaded_image_protocol->image_size));
	efi_delete_handle(&image_obj->header);

	return &ret;
}

static efi_status_t efi_exit(efi_handle_t image_handle,
				    efi_status_t exit_status,
				    efi_uintn_t exit_data_size,
				    u16 *exit_data)
{
	/*
	 * TODO: We should call the unload procedure of the loaded
	 *	 image protocol.
	 */
	efi_status_t ret;
	struct efi_loaded_image *loaded_image_protocol;
	struct efi_loaded_image_obj *image_obj =
		(struct efi_loaded_image_obj *)image_handle;
	struct jmp_buf_data *exit_jmp;

	EFI_ENTRY("%p, %ld, %zu, %p", image_handle, exit_status,
		  exit_data_size, exit_data);

	/* Check parameters */
	ret = EFI_CALL(efi_open_protocol(image_handle, &efi_guid_loaded_image,
					 (void **)&loaded_image_protocol,
					 NULL, NULL,
					 EFI_OPEN_PROTOCOL_GET_PROTOCOL));
	if (ret != EFI_SUCCESS) {
		//ret = EFI_INVALID_PARAMETER;
		goto out;
	}

	/* Exit data is only foreseen in case of failure. */
	if (exit_status != EFI_SUCCESS) {
		ret = efi_update_exit_data(image_obj, exit_data_size,
					   exit_data);
		/* Exiting has priority. Don't return error to caller. */
		if (ret != EFI_SUCCESS)
			EFI_PRINT("%s: out of memory\n", __func__);
	}
	/* efi_delete_image() frees image_obj. Copy before the call. */
	//exit_jmp = image_obj->exit_jmp;
	//*image_obj->exit_status = exit_status;
	if (image_obj->image_type == IMAGE_SUBSYSTEM_EFI_APPLICATION ||
	    exit_status != EFI_SUCCESS)
		efi_delete_image(image_obj, loaded_image_protocol);

	/* Make sure entry/exit counts for EFI world cross-overs match */
	EFI_EXIT(exit_status);

	/*
	 * But longjmp out with the U-Boot gd, not the application's, as
	 * the other end is a setjmp call inside EFI context.
	 */
	efi_restore_gd();

	//longjmp(exit_jmp, 1);
    image_obj->exit_status = exit_status;
	longjmp(&image_obj->exit_jmp, 1);

	panic("EFI application exited");
out:
	return EFI_EXIT(ret);
}