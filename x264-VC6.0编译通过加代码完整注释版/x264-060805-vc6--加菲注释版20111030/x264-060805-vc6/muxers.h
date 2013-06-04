#ifndef _X264_MUXERS_H_//MUXER:多路转换[复用]器
	#define _X264_MUXERS_H_

	typedef void *hnd_t;

	int open_file_yuv( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );/**/
	int get_frame_total_yuv( hnd_t handle );/**/
	int read_frame_yuv( x264_picture_t *p_pic, hnd_t handle, int i_frame );/**/
	int close_file_yuv( hnd_t handle );/**/

	int open_file_y4m( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );/**/
	int get_frame_total_y4m( hnd_t handle );/**/
	int read_frame_y4m( x264_picture_t *p_pic, hnd_t handle, int i_frame );/**/
	int close_file_y4m( hnd_t handle );/**/

	#ifdef AVIS_INPUT
		int open_file_avis( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );/**/
		int get_frame_total_avis( hnd_t handle );/**/
		int read_frame_avis( x264_picture_t *p_pic, hnd_t handle, int i_frame );/**/
		int close_file_avis( hnd_t handle );/**/
	#endif

	#ifdef HAVE_PTHREAD
		int open_file_thread( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );/**/
		int get_frame_total_thread( hnd_t handle );/**/
		int read_frame_thread( x264_picture_t *p_pic, hnd_t handle, int i_frame );/**/
		int close_file_thread( hnd_t handle );/**/
	#endif

	int open_file_bsf( char *psz_filename, hnd_t *p_handle );/*扩展名为.bsf的文件是一个视频文件*/
	int set_param_bsf( hnd_t handle, x264_param_t *p_param );/**/
	int write_nalu_bsf( hnd_t handle, uint8_t *p_nal, int i_size );/**/
	int set_eop_bsf( hnd_t handle,  x264_picture_t *p_picture );/**/
	int close_file_bsf( hnd_t handle );/**/

	int tst();
	/*
	输出MP4文件
	*/
	#ifdef MP4_OUTPUT
		int open_file_mp4( char *psz_filename, hnd_t *p_handle );/*打开mp4文件*/
		int set_param_mp4( hnd_t handle, x264_param_t *p_param );/*设置参数*/
		int write_nalu_mp4( hnd_t handle, uint8_t *p_nal, int i_size );/*写NALU*/
		int set_eop_mp4( hnd_t handle, x264_picture_t *p_picture );/**/
		int close_file_mp4( hnd_t handle );/* 关闭文件 */
	#endif

	/*
		
	输出MKV文件
	*/
	int open_file_mkv( char *psz_filename, hnd_t *p_handle );/*打开mkv文件*/
	int set_param_mkv( hnd_t handle, x264_param_t *p_param );/*设置参数*/
	int write_nalu_mkv( hnd_t handle, uint8_t *p_nal, int i_size );/*写NALU*/
	int set_eop_mkv( hnd_t handle, x264_picture_t *p_picture );/**/
	int close_file_mkv( hnd_t handle );/*关闭mkv文件*/

	extern int (*p_open_infile)( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );/**/
	extern int (*p_get_frame_total)( hnd_t handle );/**/
	extern int (*p_read_frame)( x264_picture_t *p_pic, hnd_t handle, int i_frame );/**/
	extern int (*p_close_infile)( hnd_t handle );/**/

#endif
